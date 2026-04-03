// Copyright Xyzto Works

#include "GameMode/Combat/AI/BattleAIComponent.h"

#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "Characters/Abilities/BattleMovementAbility.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/BattleUnitState.h"
#include "GameMode/Combat/AI/BattleAICandidateScorer.h"
#include "GameMode/Combat/AI/BattleAIBehaviorProfile.h"
#include "GameMode/Combat/AI/BattleAIQueryService.h"
#include "GameMode/Combat/Resolvers/ActionResolver.h"
#include "Level/GridManager.h"
#include "Types/CharacterTypes.h"

UBattleAIComponent::UBattleAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBattleAIComponent::Initialize(UBattleAIQueryService* InQueryService)
{
	QueryService = InQueryService;

	if (!ActionResolver)
	{
		ActionResolver = NewObject<UActionResolver>(this);
	}

	if (!CandidateScorer)
	{
		CandidateScorer = NewObject<UBattleAICandidateScorer>(this);
	}

	RefreshScoringRules();
}

void UBattleAIComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!ActionResolver)
	{
		ActionResolver = NewObject<UActionResolver>(this);
	}

	if (!CandidateScorer)
	{
		CandidateScorer = NewObject<UBattleAICandidateScorer>(this);
	}

	RefreshScoringRules();
}

void UBattleAIComponent::RefreshScoringRules()
{
	if (!CandidateScorer)
	{
		return;
	}

	CandidateScorer->SetRules(BehaviorProfile);
	
	UE_LOG(
	LogTemp,
	Warning,
	TEXT("[BattleAIComponent] %s BehaviorProfile = %s"),
	*GetNameSafe(GetOwner()),
	*GetNameSafe(BehaviorProfile)
);
}

void UBattleAIComponent::GatherCandidateActions(const FTeamTurnContext& TurnContext, TArray<FTeamCandidateAction>& OutCandidates) const
{
	const int32 InitialCount = OutCandidates.Num();

	const ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	if (!OwnerCharacter || !QueryService || !QueryService->CanUnitStillAct(OwnerCharacter))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleAIComponent] Skipping gather for %s"), *GetNameSafe(OwnerCharacter));
		return;
	}

	GatherAbilityCandidates(TurnContext, OutCandidates);
	GatherMoveCandidates(TurnContext, OutCandidates);
	GatherEndTurnCandidate(TurnContext, OutCandidates);

	const int32 AddedCount = OutCandidates.Num() - InitialCount;
	UE_LOG(LogTemp, Warning, TEXT("[BattleAIComponent] %s generated %d candidates"), *GetNameSafe(OwnerCharacter), AddedCount);
}

void UBattleAIComponent::GatherAbilityCandidates(const FTeamTurnContext& TurnContext, TArray<FTeamCandidateAction>& OutCandidates) const
{
	const ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	const UBattleModeComponent* BattleMode = OwnerCharacter ? OwnerCharacter->GetBattleModeComponent() : nullptr;
	if (!OwnerCharacter || !BattleMode || !ActionResolver || !QueryService || !OwnerCharacter->GetWorld())
	{
		return;
	}

	UGridManager* Grid = OwnerCharacter->GetWorld()->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		return;
	}

	for (const FUnitAbilityEntry& Entry : BattleMode->GetGrantedBattleAbilities())
	{
		if (!Entry.Definition || !Entry.Definition->AbilityClass || Entry.bIsMovementAbility)
			continue;

		const UBattleGameplayAbility* AbilityCDO =
			Entry.Definition->AbilityClass->GetDefaultObject<UBattleGameplayAbility>();
		if (!AbilityCDO)
		{
			continue;
		}

		const TArray<ACharacterBase*> ValidTargets = QueryService->GetValidTargetsForAbility(OwnerCharacter, Entry.Definition->AbilityClass, Entry.Definition);
		for (ACharacterBase* Target : ValidTargets)
		{
			if (!Target)
			{
				continue;
			}

			FActionContext BaseContext;
			if (!BuildBaseContext(BaseContext))
			{
				continue;
			}

			BaseContext.AbilityClass = Entry.Definition->AbilityClass;
			BaseContext.AbilityDefinition = Entry.Definition;
			BaseContext.TargetActor = Target;
			BaseContext.HoveredGridCoord = Grid->WorldToGrid(Target->GetActorLocation());
			BaseContext.HoveredWorldPosition = Grid->GridToWorld(BaseContext.HoveredGridCoord);
			BaseContext.SelectionPhase = EBattleActionSelectionPhase::SelectingTarget;

			if (AbilityCDO->GetSelectionMode() == EBattleAbilitySelectionMode::TargetThenConfirmApproach)
			{
				for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
				{
					for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
					{
						if (OffsetX == 0 && OffsetY == 0)
						{
							continue;
						}

						FActionContext ApproachContext = BaseContext;
						ApproachContext.LockedTargetActor = Target;
						ApproachContext.LockedTargetGridCoord = BaseContext.HoveredGridCoord;
						ApproachContext.bHasLockedTarget = true;
						ApproachContext.SelectionPhase = EBattleActionSelectionPhase::SelectingApproachTile;
						ApproachContext.SelectedApproachGridCoord = BaseContext.HoveredGridCoord + FIntPoint(OffsetX, OffsetY);
						ApproachContext.bHasSelectedApproachTile = true;
						ApproachContext.HoveredGridCoord = ApproachContext.SelectedApproachGridCoord;
						ApproachContext.HoveredWorldPosition = Grid->GridToWorld(ApproachContext.SelectedApproachGridCoord);
						ApproachContext.CachedPathCost = Grid->CalculatePathCost(
							ApproachContext.SourceGridCoord,
							ApproachContext.SelectedApproachGridCoord,
							ApproachContext.TraversalParams);

						AddCandidateIfValid(ApproachContext, TurnContext, OutCandidates);
					}
				}

				const int32 DistanceToTarget = FMath::Max(
					FMath::Abs(BaseContext.SourceGridCoord.X - BaseContext.HoveredGridCoord.X),
					FMath::Abs(BaseContext.SourceGridCoord.Y - BaseContext.HoveredGridCoord.Y));

				if (DistanceToTarget <= 1)
				{
					FActionContext StayContext = BaseContext;
					StayContext.LockedTargetActor = Target;
					StayContext.LockedTargetGridCoord = BaseContext.HoveredGridCoord;
					StayContext.bHasLockedTarget = true;
					StayContext.SelectionPhase = EBattleActionSelectionPhase::SelectingApproachTile;
					StayContext.SelectedApproachGridCoord = BaseContext.SourceGridCoord;
					StayContext.bHasSelectedApproachTile = true;
					StayContext.HoveredGridCoord = BaseContext.SourceGridCoord;
					StayContext.HoveredWorldPosition = OwnerCharacter->GetActorLocation();
					StayContext.CachedPathCost = 0;

					AddCandidateIfValid(StayContext, TurnContext, OutCandidates);
				}
			}
			else
			{
				BaseContext.CachedPathCost = Grid->CalculatePathCost(
					BaseContext.SourceGridCoord,
					BaseContext.HoveredGridCoord,
					BaseContext.TraversalParams);

				AddCandidateIfValid(BaseContext, TurnContext, OutCandidates);
			}
		}
	}
}

void UBattleAIComponent::GatherMoveCandidates(const FTeamTurnContext& TurnContext, TArray<FTeamCandidateAction>& OutCandidates) const
{
	const ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	const UBattleModeComponent* BattleMode = OwnerCharacter ? OwnerCharacter->GetBattleModeComponent() : nullptr;
	if (!OwnerCharacter || !BattleMode || !ActionResolver || !QueryService || !TurnContext.FocusTarget || !OwnerCharacter->GetWorld())
	{
		return;
	}

	const TSubclassOf<UBattleMovementAbility> MoveAbilityClass = BattleMode->GetEquippedMovementAbilityClass();
	if (!MoveAbilityClass)
	{
		return;
	}

	UGridManager* Grid = OwnerCharacter->GetWorld()->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		return;
	}

	const FIntPoint FocusCoord = Grid->WorldToGrid(TurnContext.FocusTarget->GetActorLocation());
	const FIntPoint SourceCoord = Grid->WorldToGrid(OwnerCharacter->GetActorLocation());
	TArray<FIntPoint> ReachableTiles = QueryService->GetReachableTiles(OwnerCharacter);
	ReachableTiles.Remove(SourceCoord);

	ReachableTiles.Sort([&FocusCoord, &TurnContext](const FIntPoint& A, const FIntPoint& B)
	{
		const int32 DistanceA = FMath::Max(FMath::Abs(A.X - FocusCoord.X), FMath::Abs(A.Y - FocusCoord.Y));
		const int32 DistanceB = FMath::Max(FMath::Abs(B.X - FocusCoord.X), FMath::Abs(B.Y - FocusCoord.Y));
		const bool bAReserved = TurnContext.ReservedTiles.Contains(A);
		const bool bBReserved = TurnContext.ReservedTiles.Contains(B);

		if (bAReserved != bBReserved)
		{
			return !bAReserved;
		}

		return DistanceA < DistanceB;
	});

	const int32 MaxCandidates = FMath::Min(3, ReachableTiles.Num());
	for (int32 Index = 0; Index < MaxCandidates; ++Index)
	{
		FActionContext Context;
		if (!BuildBaseContext(Context))
		{
			continue;
		}

		Context.AbilityClass = MoveAbilityClass;
		Context.HoveredGridCoord = ReachableTiles[Index];
		Context.HoveredWorldPosition = Grid->GridToWorld(ReachableTiles[Index]);
		Context.TargetActor = Grid->GetActorAt(ReachableTiles[Index]);
		Context.CachedPathCost = Grid->CalculatePathCost(
			Context.SourceGridCoord,
			Context.HoveredGridCoord,
			Context.TraversalParams);

		AddCandidateIfValid(Context, TurnContext, OutCandidates);
	}
}

void UBattleAIComponent::GatherEndTurnCandidate(const FTeamTurnContext& TurnContext, TArray<FTeamCandidateAction>& OutCandidates) const
{
	FTeamCandidateAction Candidate;
	Candidate.ActingUnit = Cast<ACharacterBase>(GetOwner());
	Candidate.ActionType = ETeamAIActionType::EndTurn;
	Candidate.Reason = TEXT("No useful action");
	Candidate.Score = -25.f;

	OutCandidates.Add(Candidate);
}

bool UBattleAIComponent::BuildBaseContext(FActionContext& OutContext) const
{
	ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	if (!OwnerCharacter || !OwnerCharacter->GetWorld())
	{
		return false;
	}

	UGridManager* Grid = OwnerCharacter->GetWorld()->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		return false;
	}

	UBattleModeComponent* BattleMode     = OwnerCharacter->GetBattleModeComponent();
	UBattleState*         BattleStateLocal = BattleMode ? BattleMode->GetBattleState() : nullptr;
	const FBattleUnitState* UnitState     = BattleStateLocal
		? BattleStateLocal->FindUnitState(OwnerCharacter)
		: nullptr;

	if (!UnitState)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BattleAIComponent] BuildBaseContext: BattleUnitState not found for %s"),
			*GetNameSafe(OwnerCharacter));
		return false;
	}

	OutContext = FActionContext();
	OutContext.SourceActor         = OwnerCharacter;
	OutContext.SourceWorldPosition = OwnerCharacter->GetActorLocation();
	OutContext.SourceGridCoord     = Grid->WorldToGrid(OwnerCharacter->GetActorLocation());
	OutContext.TraversalParams     = UnitState->TraversalParams;
	OutContext.SelectionPhase      = EBattleActionSelectionPhase::SelectingTarget;

	return true;
}

bool UBattleAIComponent::BuildEvalContext(const FTeamTurnContext& TurnContext, FAICandidateEvalContext& OutContext) const
{
	ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	if (!OwnerCharacter || !QueryService || !OwnerCharacter->GetWorld())
	{
		return false;
	}

	UGridManager* Grid = OwnerCharacter->GetWorld()->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		return false;
	}

	OutContext = FAICandidateEvalContext();
	OutContext.ActingUnit   = OwnerCharacter;
	OutContext.TurnContext  = &TurnContext;
	OutContext.QueryService = QueryService;
	OutContext.Grid         = Grid;

	for (AActor* Combatant : TurnContext.AllCombatants)
	{
		ACharacterBase* Character = Cast<ACharacterBase>(Combatant);
		if (!Character || Character == OwnerCharacter)
		{
			continue;
		}

		if (QueryService->AreUnitsAllies(OwnerCharacter, Character))
		{
			OutContext.Allies.Add(Character);
		}
		else
		{
			OutContext.Enemies.Add(Character);
		}
	}

	return true;
}

void UBattleAIComponent::AddCandidateIfValid(const FActionContext& Context, const FTeamTurnContext& TurnContext, TArray<FTeamCandidateAction>& OutCandidates) const
{
	if (!ActionResolver || !Context.AbilityClass)
	{
		return;
	}

	const FActionResult ExecutionResult = ActionResolver->ResolveExecution(Context.AbilityClass, Context);
	if (!ExecutionResult.bIsValid)
	{
		return;
	}

	const ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	// AP check reads from BattleUnitState — the authority during combat
	UBattleModeComponent* BattleMode      = OwnerCharacter->GetBattleModeComponent();
	UBattleState*         BattleStateLocal = BattleMode ? BattleMode->GetBattleState() : nullptr;
	const FBattleUnitState* UnitState     = BattleStateLocal
		? BattleStateLocal->FindUnitState(const_cast<ACharacterBase*>(OwnerCharacter))
		: nullptr;

	if (!UnitState || ExecutionResult.ActionPointsCost > UnitState->CurrentActionPoints)
	{
		return;
	}

	FTeamCandidateAction Candidate;
	Candidate.ActingUnit = const_cast<ACharacterBase*>(OwnerCharacter);
	Candidate.AbilityClass = Context.AbilityClass;
	Candidate.AbilityDefinition = Context.AbilityDefinition;
	Candidate.ExpectedAPCost = ExecutionResult.ActionPointsCost;
	Candidate.ExpectedDamage = ExecutionResult.ExpectedDamage;
	Candidate.Targeting.TargetActor = Cast<ACharacterBase>(
		ExecutionResult.ResolvedAttackTarget ? ExecutionResult.ResolvedAttackTarget : Context.TargetActor);
	Candidate.Targeting.bHasActorTarget = Candidate.Targeting.TargetActor != nullptr;

	// TargetTile and bHasTileTarget are only meaningful for movement or approach-tile phases.
	// Direct-target abilities (ranged, AoE, single-confirm) resolve via TargetActor only —
	// setting bHasTileTarget=true here would cause the executor to enter the approach-tile branch
	// and overwrite HoveredGridCoord with (0,0), breaking RangeRule validation on execution.
	Candidate.Targeting.TargetTile = FIntPoint::ZeroValue;
	Candidate.Targeting.bHasTileTarget = false;

	if (Context.AbilityClass->IsChildOf(UBattleMovementAbility::StaticClass()))
	{
		Candidate.ActionType = ETeamAIActionType::Move;
		Candidate.Targeting.TargetTile = Context.HoveredGridCoord;
		Candidate.Targeting.bHasTileTarget = true;
		Candidate.Reason = TEXT("Reposition");
	}
	else
	{
		Candidate.ActionType = ETeamAIActionType::UseAbility;

		// Only set a TargetTile when we have an explicit approach tile chosen by the AI.
		// For SelectingTarget phase (ranged, AoE, single-confirm), TargetActor is sufficient.
		if (Context.SelectionPhase == EBattleActionSelectionPhase::SelectingApproachTile)
		{
			Candidate.Targeting.TargetTile = Context.SelectedApproachGridCoord;
			Candidate.Targeting.bHasTileTarget = true;
		}

		Candidate.Reason = TEXT("Use ability");
	}

	FAICandidateEvalContext EvalContext;
	if (BuildEvalContext(TurnContext, EvalContext) && CandidateScorer)
	{
		Candidate.Score = CandidateScorer->ScoreCandidate(Candidate, EvalContext);
	}
	else
	{
		Candidate.Score = -1000.f;
	}

	OutCandidates.Add(Candidate);
}

void UBattleAIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}