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
#include "Types/BattleTypes.h"

UGA_Attack_Ranged_LOS::UGA_Attack_Ranged_LOS()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(TAG_Ability_Attack_Ranged);
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(TAG_State_Stunned);

	TargetPolicy.AddRule(MakeUnique<FactionRule>(EFactionType::Enemy));
	TargetPolicy.AddRule(MakeUnique<RangeRule>(MaxRange));
	TargetPolicy.AddRule(MakeUnique<LineOfSightRule>());

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

	ACharacterBase* SourceCharacter = Cast<ACharacterBase>(Context.SourceActor);
	ACharacterBase* TargetCharacter = Cast<ACharacterBase>(Context.TargetActor);

	UE_LOG(LogTemp, Warning, TEXT("[Ranged] SourceActor: %s"), *GetNameSafe(SourceCharacter));
	UE_LOG(LogTemp, Warning, TEXT("[Ranged] TargetActor: %s"), *GetNameSafe(TargetCharacter));
	UE_LOG(LogTemp, Warning, TEXT("[Ranged] SourceCoord: %s"), *Context.SourceGridCoord.ToString());
	UE_LOG(LogTemp, Warning, TEXT("[Ranged] HoveredCoord: %s"), *Context.HoveredGridCoord.ToString());

	if (!SourceCharacter || !TargetCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Ranged] Failed: no source or target"));
		return false;
	}

	if (!TargetPolicy.IsValid(Context))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Ranged] Failed: policy invalid"));
		return false;
	}

	UUnitStatsComponent* SourceStats = SourceCharacter->GetUnitStats();
	UUnitStatsComponent* TargetStats = TargetCharacter->GetUnitStats();

	if (!SourceStats || !TargetStats || !TargetStats->IsAlive())
	{
		return false;
	}

	OutResult.bIsValid = true;
	OutResult.bRequiresMovement = false;
	OutResult.ActionPointsCost = BaseActionPointsCost;
	OutResult.ResolvedAttackTarget = TargetCharacter;
	OutResult.ExpectedDamage = SourceStats->GetRangedAttack() * BaseDamageMultiplier;
	OutResult.HitChance = 1.0f;
	OutResult.AffectedActors.Add(TargetCharacter);

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
	if (!CachedCharacter || !CachedTargetCharacter)
	{
		return;
	}

	UUnitStatsComponent* SourceStats = CachedCharacter->GetUnitStats();
	UUnitStatsComponent* TargetStats = CachedTargetCharacter->GetUnitStats();

	if (!SourceStats || !TargetStats || !TargetStats->IsAlive())
	{
		return;
	}

	// Route through the event bus so BattleState, floating text,
	// and all other listeners are notified in one place
	UBattleModeComponent* BattleMode = CachedCharacter->GetBattleModeComponent();
	if (UCombatEventBus* Bus = BattleMode ? BattleMode->GetCombatEventBus() : nullptr)
	{
		Bus->NotifyDamageApplied(
			CachedCharacter,
			CachedTargetCharacter,
			SourceStats->GetRangedAttack() * BaseDamageMultiplier
		);
		return;
	}

	UE_LOG(LogTemp, Error,
		TEXT("[GA_Attack_Ranged_LOS] CombatEventBus not found — damage not applied for %s"),
		*CachedCharacter->GetName()
	);
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
		BattleMode->ConsumeActionPoints(CachedActionPointsCost);
	}
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