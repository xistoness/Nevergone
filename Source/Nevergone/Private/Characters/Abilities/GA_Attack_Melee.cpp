// Copyright Xyzto Works

#include "Characters/Abilities/GA_Attack_Melee.h"

#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/AbilityPreview/Preview_Attack_Melee.h"
#include "Characters/Abilities/BattleMovementAbility.h"
#include "Characters/Abilities/TargetingRules/FactionRule.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "Level/GridManager.h"
#include "Nevergone.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/CombatFormulas.h"
#include "Types/BattleTypes.h"

UGA_Attack_Melee::UGA_Attack_Melee()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(TAG_Ability_Attack_Melee);
	SetAssetTags(AssetTags);

	PreviewRendererClass = UPreview_Attack_Melee::StaticClass();
}

EBattleAbilitySelectionMode UGA_Attack_Melee::GetSelectionMode() const
{
	return EBattleAbilitySelectionMode::TargetThenConfirmApproach;
}

FActionResult UGA_Attack_Melee::BuildPreview(const FActionContext& Context) const
{
	FActionResult Result;
	BuildAttackResult(Context, Result);
	return Result;
}

FActionResult UGA_Attack_Melee::BuildExecution(const FActionContext& Context) const
{
	FActionResult Result;
	BuildAttackResult(Context, Result);
	return Result;
}

bool UGA_Attack_Melee::BuildAttackResult(const FActionContext& Context, FActionResult& OutResult) const
{
	OutResult = FActionResult();
	
	if (!HasSufficientAP(Context))
	{
		return false;
	}

	ACharacterBase* SourceCharacter = Cast<ACharacterBase>(Context.SourceActor);

	ACharacterBase* TargetCharacter = nullptr;
	FIntPoint TargetGridCoord = FIntPoint::ZeroValue;

	if (Context.SelectionPhase == EBattleActionSelectionPhase::SelectingTarget)
	{
		TargetCharacter = Cast<ACharacterBase>(Context.TargetActor);
		TargetGridCoord = Context.HoveredGridCoord;
	}
	else if (Context.SelectionPhase == EBattleActionSelectionPhase::SelectingApproachTile)
	{
		TargetCharacter = Cast<ACharacterBase>(Context.LockedTargetActor);
		TargetGridCoord = Context.LockedTargetGridCoord;
	}
	else
	{
		return false;
	}

	if (!SourceCharacter || !TargetCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Attack_Melee]: No SourceCharacter or NoTargetCharacter"));
		return false;
	}

	if (!IsTargetValidForMelee(SourceCharacter, TargetCharacter))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Attack_Melee]: Target is not valid for melee attack"));
		return false;
	}

	UBattleModeComponent* BattleMode    = SourceCharacter->GetBattleModeComponent();
	UBattleState*         BattleStateRef = BattleMode ? BattleMode->GetBattleState() : nullptr;
	const FBattleUnitState* SourceState = BattleStateRef ? BattleStateRef->FindUnitState(SourceCharacter) : nullptr;
	const FBattleUnitState* TargetState = BattleStateRef ? BattleStateRef->FindUnitState(TargetCharacter)  : nullptr;

	// BattleUnitState is the authority — UnitStatsComponent is not consulted for alive check
	if (!SourceState || !TargetState || !TargetState->IsAlive())
	{
		return false;
	}

	// Phase 1: validate target and compute preview stats
	if (Context.SelectionPhase == EBattleActionSelectionPhase::SelectingTarget)
	{
		OutResult.bIsValid = true;
		OutResult.ResolvedAttackTarget = TargetCharacter;
		OutResult.AffectedActors.Add(TargetCharacter);

		if (SourceState && TargetState && AbilityDefinition)
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

	// Phase 2: player must explicitly choose the approach tile
	if (!TryBuildManualApproachResult(Context, SourceCharacter, TargetCharacter, OutResult))
	{
		return false;
	}

	OutResult.ExpectedDamage = (SourceState && TargetState && AbilityDefinition)
			? FCombatFormulas::EstimateDamage(*SourceState, *TargetState,
				AbilityDefinition->DamageSource, AbilityDefinition->DamageMultiplier,
				AbilityDefinition->BaseHitChance, AbilityDefinition->bCanMiss)
			: 0.f;

	return OutResult.bIsValid;
}

bool UGA_Attack_Melee::TryBuildManualApproachResult(
	const FActionContext& Context,
	ACharacterBase* SourceCharacter,
	ACharacterBase* TargetCharacter,
	FActionResult& OutResult
) const
{
	if (!SourceCharacter || !TargetCharacter || !Context.bHasLockedTarget)
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

	const FIntPoint CandidateCoord = Context.SelectedApproachGridCoord;

	// The chosen tile must be adjacent to the locked target
	if (!IsAdjacent(CandidateCoord, Context.LockedTargetGridCoord))
	{
		return false;
	}

	// If the player selected the current tile, attack from current position
	if (CandidateCoord == Context.SourceGridCoord)
	{
		OutResult.bIsValid = true;
		OutResult.bRequiresMovement = false;
		OutResult.MovementTargetGridCoord = Context.SourceGridCoord;
		OutResult.MovementTargetWorldPosition = SourceCharacter->GetActorLocation();
		OutResult.ActionPointsCost = GetActionPointsCost();
		OutResult.ResolvedAttackTarget = TargetCharacter;
		return true;
	}

	UBattleModeComponent* BattleMode = SourceCharacter->GetBattleModeComponent();
	if (!BattleMode)
	{
		return false;
	}

	const TSubclassOf<UBattleMovementAbility> MovementClass =
		BattleMode->GetEquippedMovementAbilityClass();

	if (!MovementClass)
	{
		return false;
	}

	const UBattleMovementAbility* MovementCDO =
		MovementClass->GetDefaultObject<UBattleMovementAbility>();

	if (!MovementCDO)
	{
		return false;
	}

	FActionContext MovementContext{};
	MovementContext.SourceActor = Context.SourceActor;

	// IMPORTANT: this must refer to the candidate tile occupant, not the locked target
	MovementContext.TargetActor = Grid->GetActorAt(CandidateCoord);

	MovementContext.SourceGridCoord = Context.SourceGridCoord;
	MovementContext.HoveredGridCoord = CandidateCoord;
	MovementContext.MovementTargetGridCoord = CandidateCoord;
	MovementContext.SourceWorldPosition = Context.SourceWorldPosition;
	MovementContext.HoveredWorldPosition = Grid->GridToWorld(CandidateCoord);
	MovementContext.MovementTargetWorldPosition = Grid->GridToWorld(CandidateCoord);
	MovementContext.TraversalParams = Context.TraversalParams;
	MovementContext.CachedPathCost = Grid->CalculatePathCost(
		Context.SourceGridCoord,
		CandidateCoord,
		Context.TraversalParams
	);

	if (MovementContext.CachedPathCost == INDEX_NONE || MovementContext.CachedPathCost < 0)
	{
		return false;
	}

	const FActionResult MovementResult = MovementCDO->BuildMovementExecution(MovementContext);
	if (!MovementResult.bIsValid)
	{
		return false;
	}

	OutResult = MovementResult;
	OutResult.bIsValid = true;
	OutResult.bRequiresMovement = true;
	OutResult.ActionPointsCost = FMath::Max(GetActionPointsCost(), MovementResult.ActionPointsCost);
	OutResult.ResolvedAttackTarget = TargetCharacter;

	return true;
}

bool UGA_Attack_Melee::TryBuildApproachResult(
	const FActionContext& Context,
	ACharacterBase* SourceCharacter,
	ACharacterBase* TargetCharacter,
	FActionResult& OutResult
) const
{
	if (!SourceCharacter || !TargetCharacter)
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

	UBattleModeComponent* BattleMode = SourceCharacter->GetBattleModeComponent();
	if (!BattleMode)
	{
		return false;
	}

	const TSubclassOf<UBattleMovementAbility> MovementClass =
		BattleMode->GetEquippedMovementAbilityClass();

	if (!MovementClass)
	{
		return false;
	}

	const UBattleMovementAbility* MovementCDO =
		MovementClass->GetDefaultObject<UBattleMovementAbility>();

	if (!MovementCDO)
	{
		return false;
	}

	const TArray<FIntPoint> AdjacentCoords = GetAdjacentCoords(Context.HoveredGridCoord);

	bool bFoundAny = false;
	FActionResult BestMovementResult;
	int32 BestPathLength = TNumericLimits<int32>::Max();

	for (const FIntPoint& CandidateCoord : AdjacentCoords)
	{
		FActionContext MovementContext{};
		MovementContext.SourceActor = Context.SourceActor;
		MovementContext.TargetActor = Grid->GetActorAt(CandidateCoord);
		MovementContext.SourceGridCoord = Context.SourceGridCoord;
		MovementContext.HoveredGridCoord = CandidateCoord;
		MovementContext.MovementTargetGridCoord = CandidateCoord;
		MovementContext.SourceWorldPosition = Context.SourceWorldPosition;
		MovementContext.HoveredWorldPosition = Grid->GridToWorld(CandidateCoord);
		MovementContext.MovementTargetWorldPosition = Grid->GridToWorld(CandidateCoord);
		MovementContext.TraversalParams = Context.TraversalParams;
		MovementContext.CachedPathCost = Grid->CalculatePathCost(
			Context.SourceGridCoord,
			CandidateCoord,
			Context.TraversalParams
		);

		if (MovementContext.CachedPathCost == INDEX_NONE || MovementContext.CachedPathCost < 0)
		{
			continue;
		}

		const FActionResult MovementResult = MovementCDO->BuildMovementExecution(MovementContext);
		if (!MovementResult.bIsValid)
		{
			continue;
		}
		const int32 TotalAPCost = FMath::Max(GetActionPointsCost(), MovementResult.ActionPointsCost);
		const int32 PathLength = MovementResult.PathPoints.Num();

		if (!bFoundAny
			|| TotalAPCost < OutResult.ActionPointsCost
			|| (TotalAPCost == OutResult.ActionPointsCost && PathLength < BestPathLength))
		{
			bFoundAny = true;
			BestPathLength = PathLength;

			OutResult = MovementResult;
			OutResult.bIsValid = true;
			OutResult.bRequiresMovement = true;
			OutResult.ActionPointsCost = TotalAPCost;
			OutResult.ResolvedAttackTarget = TargetCharacter;
		}
	}

	return bFoundAny;
}

bool UGA_Attack_Melee::IsTargetValidForMelee(
	ACharacterBase* SourceCharacter,
	ACharacterBase* TargetCharacter
) const
{
	if (!SourceCharacter || !TargetCharacter) { return false; }

	FActionContext ValidationContext{};
	ValidationContext.SourceActor = SourceCharacter;
	ValidationContext.TargetActor = TargetCharacter;

	UGridManager* Grid = SourceCharacter->GetWorld()
		? SourceCharacter->GetWorld()->GetSubsystem<UGridManager>()
		: nullptr;

	if (Grid)
	{
		ValidationContext.SourceGridCoord  = Grid->WorldToGrid(SourceCharacter->GetActorLocation());
		ValidationContext.HoveredGridCoord = Grid->WorldToGrid(TargetCharacter->GetActorLocation());
	}

	// Build faction rule from AbilityDefinition — not hardcoded
	CompositeTargetingPolicy LivePolicy;
	LivePolicy.AddRule(MakeUnique<FactionRule>(
		GetTargetFaction() == EAbilityTargetFaction::Ally ? EFactionType::Ally : EFactionType::Enemy
	));

	return LivePolicy.IsValid(ValidationContext);
}

bool UGA_Attack_Melee::IsAdjacent(const FIntPoint& Source, const FIntPoint& Target) const
{
	const int32 Distance = FMath::Max(
		FMath::Abs(Source.X - Target.X),
		FMath::Abs(Source.Y - Target.Y)
	);

	return Distance <= GetMaxRange();
}

TArray<FIntPoint> UGA_Attack_Melee::GetAdjacentCoords(const FIntPoint& Center) const
{
	TArray<FIntPoint> Result;
	Result.Reserve(8);

	for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
	{
		for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
		{
			if (OffsetX == 0 && OffsetY == 0)
			{
				continue;
			}

			Result.Add(FIntPoint(Center.X + OffsetX, Center.Y + OffsetY));
		}
	}

	return Result;
}

void UGA_Attack_Melee::ActivateAbility(
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
	bCachedRequiredMovement = ExecutionResult.bRequiresMovement;
	CachedFinalGridCoord = ExecutionResult.bRequiresMovement
		? ExecutionResult.MovementTargetGridCoord
		: Context.SourceGridCoord;

	if (!CachedTargetCharacter)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!ExecutionResult.bRequiresMovement)
	{
		ApplyResolvedAttack();
		FinalizeAttackAction();
		return;
	}

	const TSubclassOf<UBattleMovementAbility> MovementClass =
		BattleMode->GetEquippedMovementAbilityClass();

	if (!MovementClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveMovementExecutor = NewObject<UBattleMovementAbility>(this, MovementClass);
	if (!ActiveMovementExecutor)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FSimpleDelegate MovementFinishedDelegate;
	MovementFinishedDelegate.BindUObject(this, &UGA_Attack_Melee::HandleApproachMovementFinished);

	if (!ActiveMovementExecutor->ExecuteMovementPlan(
		SourceCharacter,
		ExecutionResult,
		MovementFinishedDelegate))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UGA_Attack_Melee::HandleApproachMovementFinished()
{
	ApplyResolvedAttack();
	FinalizeAttackAction();
}

void UGA_Attack_Melee::ApplyResolvedAttack()
{
	if (!CachedCharacter || !CachedTargetCharacter) { return; }
 
	UBattleModeComponent* BattleMode     = CachedCharacter->GetBattleModeComponent();
	UBattleState*         BattleStateRef = BattleMode ? BattleMode->GetBattleState() : nullptr;
	const FBattleUnitState* SourceState  = BattleStateRef ? BattleStateRef->FindUnitState(CachedCharacter)       : nullptr;
	const FBattleUnitState* TargetState  = BattleStateRef ? BattleStateRef->FindUnitState(CachedTargetCharacter) : nullptr;
 
	if (!SourceState || !TargetState)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GA_Attack_Melee] ApplyResolvedAttack: BattleUnitState not found — damage not applied"));
		return;
	}
 
	const float FinalDamage = ResolveAttackDamage(*SourceState, *TargetState);
	
	PlayCastSFX(CachedCharacter);
	PlayImpactSFX(CachedTargetCharacter);
 
	UCombatEventBus* Bus = BattleMode ? BattleMode->GetCombatEventBus() : nullptr;
	if (!Bus)
	{
		UE_LOG(LogTemp, Error, TEXT("[GA_Attack_Melee] CombatEventBus not found"));
		return;
	}
 
	// EventBus notifies consequences (HP mutation, death check, floating text)
	// The SFX above play the *action* — EventBus DefaultHitSFX plays the *reaction*
	Bus->NotifyDamageApplied(CachedCharacter, CachedTargetCharacter, FinalDamage);
}

void UGA_Attack_Melee::FinalizeAttackAction()
{
	FinalizeAbilityExecution();
}

void UGA_Attack_Melee::ApplyAbilityCompletionEffects()
{
	Super::ApplyAbilityCompletionEffects();

	if (!CachedCharacter) { return; }

	UBattleModeComponent* BattleMode = CachedCharacter->GetBattleModeComponent();
	if (!BattleMode) { return; }

	if (bCachedRequiredMovement)
	{
		BattleMode->UpdateOccupancy(CachedFinalGridCoord);
	}

	if (UBattleState* BattleStateRef = BattleMode->GetBattleState())
	{
		BattleStateRef->ConsumeActionPoints(CachedCharacter, CachedActionPointsCost);
	}

	// Apply on-hit effects to the target (e.g. stun configured in AbilityDefinition)
	if (CachedTargetCharacter)
	{
		ApplyOnHitStatusEffects(CachedCharacter, { CachedTargetCharacter });
	}
	ApplySelfStatusEffects(CachedCharacter);

	TryStartCooldown(CachedCharacter);
}

void UGA_Attack_Melee::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	ActiveMovementExecutor = nullptr;
	CachedCharacter = nullptr;
	CachedTargetCharacter = nullptr;
	CachedActionPointsCost = 0;
	bCachedRequiredMovement = false;
	CachedFinalGridCoord = FIntPoint::ZeroValue;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}