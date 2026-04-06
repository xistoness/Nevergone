// Copyright Xyzto Works

#include "Characters/Abilities/GA_Attack_Ranged_LOS.h"

#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/TargetingRules/FactionRule.h"
#include "Characters/Abilities/TargetingRules/LineOfSightRule.h"
#include "Characters/Abilities/TargetingRules/RangeRule.h"
#include "Characters/Abilities/AbilityPreview/Preview_Attack_Ranged.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "Level/GridManager.h"
#include "Nevergone.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/CombatFormulas.h"
#include "Types/BattleTypes.h"

UGA_Attack_Ranged_LOS::UGA_Attack_Ranged_LOS()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(TAG_Ability_Attack_Ranged);
	SetAssetTags(AssetTags);

	PreviewRendererClass = UPreview_Attack_Ranged::StaticClass();
}

FActionResult UGA_Attack_Ranged_LOS::BuildPreview(const FActionContext& Context) const
{
	FActionResult Result;
	BuildAttackResult(Context, Result);
	return Result;
}

FActionResult UGA_Attack_Ranged_LOS::BuildExecution(const FActionContext& Context) const
{
	FActionResult Result;
	BuildAttackResult(Context, Result);
	return Result;
}

bool UGA_Attack_Ranged_LOS::BuildAttackResult(const FActionContext& Context, FActionResult& OutResult) const
{
	OutResult = FActionResult();
	
	if (!HasSufficientAP(Context))
	{
		return false;
	}

    ACharacterBase* SourceCharacter = Cast<ACharacterBase>(Context.SourceActor);
    ACharacterBase* TargetCharacter = Cast<ACharacterBase>(Context.TargetActor);

    if (!SourceCharacter || !TargetCharacter)
    {
        UE_LOG(LogTemp, Log, TEXT("[GA_Attack_Ranged_LOS] BuildAttackResult: no source or target"));
        return false;
    }

    // Build targeting policy here, not in the constructor, so AbilityDefinition
    // values are live. GetMaxRange() and GetTargetFaction() read from AbilityDefinition.
    CompositeTargetingPolicy LivePolicy;
    LivePolicy.AddRule(MakeUnique<FactionRule>(
        GetTargetFaction() == EAbilityTargetFaction::Ally ? EFactionType::Ally : EFactionType::Enemy
    ));
    LivePolicy.AddRule(MakeUnique<RangeRule>(GetMaxRange()));
    LivePolicy.AddRule(MakeUnique<LineOfSightRule>());

    if (!LivePolicy.IsValid(Context))
    {
        UE_LOG(LogTemp, Log, TEXT("[GA_Attack_Ranged_LOS] BuildAttackResult: policy invalid"));
        return false;
    }

    UBattleModeComponent* BattleMode     = SourceCharacter->GetBattleModeComponent();
    UBattleState*         BattleStateRef = BattleMode ? BattleMode->GetBattleState() : nullptr;
    const FBattleUnitState* SourceState  = BattleStateRef ? BattleStateRef->FindUnitState(SourceCharacter) : nullptr;
    const FBattleUnitState* TargetState  = BattleStateRef ? BattleStateRef->FindUnitState(TargetCharacter)  : nullptr;

    if (!SourceState || !TargetState || !TargetState->IsAlive())
    {
        return false;
    }

    OutResult.bIsValid             = true;
    OutResult.bRequiresMovement    = false;
    OutResult.ActionPointsCost     = GetActionPointsCost();
    OutResult.ResolvedAttackTarget = TargetCharacter;
    OutResult.AffectedActors.Add(TargetCharacter);

    if (AbilityDefinition)
    {
        OutResult.ExpectedDamage = FCombatFormulas::EstimateDamage(
            *SourceState, *TargetState,
            AbilityDefinition->DamageSource,
            AbilityDefinition->DamageMultiplier,
            AbilityDefinition->BaseHitChance,
            AbilityDefinition->bCanMiss
        );
        OutResult.HitChance = FCombatFormulas::ComputeHitChance(
            *SourceState, *TargetState,
            AbilityDefinition->BaseHitChance,
            AbilityDefinition->bCanMiss
        );
    }

    return true;
}

void UGA_Attack_Ranged_LOS::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacterBase* SourceCharacter = Cast<ACharacterBase>(ActorInfo->AvatarActor.Get());
	if (!SourceCharacter)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UBattleModeComponent* BattleMode = SourceCharacter->GetBattleModeComponent();
	if (!BattleMode)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FActionContext& Context = BattleMode->GetCurrentContext();
	const FActionResult ExecutionResult = BuildExecution(Context);

	if (!ExecutionResult.bIsValid)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedCharacter = SourceCharacter;
	CachedTargetCharacter = Cast<ACharacterBase>(ExecutionResult.ResolvedAttackTarget);
	CachedActionPointsCost = ExecutionResult.ActionPointsCost;

	if (!CachedTargetCharacter)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ApplyResolvedAttack();
	FinalizeAttackAction();
}

void UGA_Attack_Ranged_LOS::ApplyResolvedAttack()
{
	if (!CachedCharacter || !CachedTargetCharacter) { return; }

	UBattleModeComponent* BattleMode     = CachedCharacter->GetBattleModeComponent();
	UBattleState*         BattleStateRef = BattleMode ? BattleMode->GetBattleState() : nullptr;
	const FBattleUnitState* SourceState  = BattleStateRef ? BattleStateRef->FindUnitState(CachedCharacter)       : nullptr;
	const FBattleUnitState* TargetState  = BattleStateRef ? BattleStateRef->FindUnitState(CachedTargetCharacter) : nullptr;

	if (!SourceState || !TargetState)
	{
		UE_LOG(LogTemp, Error, TEXT("[GA_Attack_Ranged_LOS] BattleUnitState not found — damage not applied"));
		return;
	}

	const float FinalDamage = ResolveAttackDamage(*SourceState, *TargetState);
	
	PlayCastSFX(CachedCharacter);
	PlayImpactSFX(CachedTargetCharacter);

	UCombatEventBus* Bus = BattleMode ? BattleMode->GetCombatEventBus() : nullptr;
	if (!Bus)
	{
		UE_LOG(LogTemp, Error, TEXT("[GA_Attack_Ranged_LOS] CombatEventBus not found"));
		return;
	}

	Bus->NotifyDamageApplied(CachedCharacter, CachedTargetCharacter, FinalDamage);

	// Apply on-hit status effects (e.g. Poison, Stun) defined in AbilityDefinition->OnHitStatusEffects.
	// Must be called after damage so effects that check IsAlive() on the target behave correctly.
	ApplyOnHitStatusEffects(CachedCharacter, { CachedTargetCharacter });
}

void UGA_Attack_Ranged_LOS::FinalizeAttackAction()
{
	FinalizeAbilityExecution();
}

void UGA_Attack_Ranged_LOS::ApplyAbilityCompletionEffects()
{
	Super::ApplyAbilityCompletionEffects();

	if (!CachedCharacter)
	{
		return;
	}

	if (UBattleModeComponent* BattleMode = CachedCharacter->GetBattleModeComponent())
	{
		if (UBattleState* BattleStateRef = BattleMode->GetBattleState())
		{
			BattleStateRef->ConsumeActionPoints(CachedCharacter, CachedActionPointsCost);
		}
	}

	// Apply self-targeting status effects defined in AbilityDefinition->SelfStatusEffects
	// (e.g. self-buff triggered on use of this ability).
	ApplySelfStatusEffects(CachedCharacter);
}

void UGA_Attack_Ranged_LOS::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	CachedCharacter = nullptr;
	CachedTargetCharacter = nullptr;
	CachedActionPointsCost = 0;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}