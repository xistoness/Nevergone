// Copyright Xyzto Works

#include "Characters/Abilities/GA_Heal_AoE.h"

#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/TargetingRules/RangeRule.h"
#include "Characters/Abilities/AbilityPreview/Preview_AOE.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "Level/GridManager.h"
#include "Nevergone.h"
#include "Types/BattleTypes.h"

UGA_Heal_AoE::UGA_Heal_AoE()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(TAG_Ability_Heal_AoE);
	SetAssetTags(AssetTags);

	ActivationBlockedTags.AddTag(TAG_State_Stunned);

	PreviewRendererClass = UPreview_AOE::StaticClass();
}

FActionResult UGA_Heal_AoE::BuildPreview(const FActionContext& Context) const
{
	FActionResult Result;
	BuildHealResult(Context, Result);
	return Result;
}

FActionResult UGA_Heal_AoE::BuildExecution(const FActionContext& Context) const
{
	FActionResult Result;
	BuildHealResult(Context, Result);
	return Result;
}

bool UGA_Heal_AoE::BuildHealResult(const FActionContext& Context, FActionResult& OutResult) const
{
	OutResult = FActionResult();

	ACharacterBase* SourceCharacter = Cast<ACharacterBase>(Context.SourceActor);
	UE_LOG(LogTemp, Warning, TEXT("[Heal] SourceActor: %s"), *GetNameSafe(SourceCharacter));
	UE_LOG(LogTemp, Warning, TEXT("[Heal] SourceCoord: %s"), *Context.SourceGridCoord.ToString());
	UE_LOG(LogTemp, Warning, TEXT("[Heal] HoveredCoord: %s"), *Context.HoveredGridCoord.ToString());

	if (!SourceCharacter) { UE_LOG(LogTemp, Warning, TEXT("[Heal] Failed: no source")); return false; }

	CompositeTargetingPolicy Policy;
	Policy.AddRule(MakeUnique<RangeRule>(CastRange));
	UE_LOG(LogTemp, Warning, TEXT("[Heal] CastRange: %d"), CastRange);

	if (!Policy.IsValid(Context)) { UE_LOG(LogTemp, Warning, TEXT("[Heal] Failed: range policy")); return false; }

	TArray<ACharacterBase*> AffectedAllies;
	if (!GatherAffectedAllies(SourceCharacter, Context.HoveredGridCoord, AffectedAllies))
	{
		return false;
	}

	if (AffectedAllies.Num() == 0)
	{
		return false;
	}

	OutResult.bIsValid = true;
	OutResult.bRequiresMovement = false;
	OutResult.ActionPointsCost = BaseActionPointsCost;
	OutResult.MovementTargetGridCoord = Context.HoveredGridCoord;

	for (ACharacterBase* Ally : AffectedAllies)
	{
		OutResult.AffectedActors.Add(Ally);
	}

	// For now this is just a placeholder because FActionResult is attack-oriented.
	// Later it would be better to add ExpectedHealing to FActionResult.
	OutResult.ExpectedDamage = 0.0f;
	OutResult.HitChance = 1.0f;

	return true;
}

bool UGA_Heal_AoE::GatherAffectedAllies(
	ACharacterBase* SourceCharacter,
	const FIntPoint& CenterCoord,
	TArray<ACharacterBase*>& OutAllies
) const
{
	if (!SourceCharacter)
	{
		return false;
	}

	UWorld* World = SourceCharacter->GetWorld();
	if (!World)
	{
		return false;
	}

	UGridManager* Grid = World->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		return false;
	}

	UUnitStatsComponent* SourceStats = SourceCharacter->GetUnitStats();
	if (!SourceStats)
	{
		return false;
	}

	for (int32 OffsetX = -HealRadius; OffsetX <= HealRadius; ++OffsetX)
	{
		for (int32 OffsetY = -HealRadius; OffsetY <= HealRadius; ++OffsetY)
		{
			const FIntPoint CandidateCoord(CenterCoord.X + OffsetX, CenterCoord.Y + OffsetY);
			AActor* Occupant = Grid->GetActorAt(CandidateCoord);
			ACharacterBase* CandidateCharacter = Cast<ACharacterBase>(Occupant);

			if (!CandidateCharacter)
			{
				continue;
			}

			UUnitStatsComponent* CandidateStats = CandidateCharacter->GetUnitStats();
			if (!CandidateStats || !CandidateStats->IsAlive())
			{
				continue;
			}

			if (CandidateStats->GetAllyTeam() != SourceStats->GetAllyTeam())
			{
				continue;
			}

			OutAllies.Add(CandidateCharacter);
		}
	}

	return true;
}

void UGA_Heal_AoE::ActivateAbility(
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
	CachedActionPointsCost = ExecutionResult.ActionPointsCost;
	CachedCenterCoord = Context.HoveredGridCoord;
	CachedAffectedAllies.Reset();

	for (AActor* Actor : ExecutionResult.AffectedActors)
	{
		if (ACharacterBase* Ally = Cast<ACharacterBase>(Actor))
		{
			CachedAffectedAllies.Add(Ally);
		}
	}

	if (CachedAffectedAllies.Num() == 0)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ApplyResolvedHeal();
	FinalizeHealAction();
}

void UGA_Heal_AoE::ApplyResolvedHeal()
{
	// Resolve the bus once before the loop — all targets share the same source
	UBattleModeComponent* BattleMode = CachedCharacter
		? CachedCharacter->GetBattleModeComponent()
		: nullptr;

	UCombatEventBus* Bus = BattleMode ? BattleMode->GetCombatEventBus() : nullptr;

	if (!Bus)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GA_Heal_AoE] CombatEventBus not found — heal not applied for %s"),
			*GetNameSafe(CachedCharacter)
		);
		return;
	}

	for (ACharacterBase* Ally : CachedAffectedAllies)
	{
		if (!Ally)
		{
			continue;
		}

		UUnitStatsComponent* Stats = Ally->GetUnitStats();
		if (!Stats || !Stats->IsAlive())
		{
			continue;
		}

		// Route through the event bus so BattleState, floating text,
		// and all other listeners are notified in one place
		Bus->NotifyHealApplied(CachedCharacter, Ally, HealAmount);
	}
}

void UGA_Heal_AoE::FinalizeHealAction()
{
	FinalizeAbilityExecution();
}

void UGA_Heal_AoE::ApplyAbilityCompletionEffects()
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

void UGA_Heal_AoE::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	CachedCharacter = nullptr;
	CachedAffectedAllies.Reset();
	CachedActionPointsCost = 0;
	CachedCenterCoord = FIntPoint::ZeroValue;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}