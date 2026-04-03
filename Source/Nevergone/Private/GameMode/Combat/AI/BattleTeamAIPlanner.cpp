// Copyright Xyzto Works

#include "GameMode/Combat/AI/BattleTeamAIPlanner.h"

#include "GameMode/Combat/AI/BattleAIActionExecutor.h"
#include "GameMode/Combat/AI/BattleAIComponent.h"
#include "GameMode/Combat/AI/BattleAIQueryService.h"
#include "GameMode/CombatManager.h"
#include "Level/GridManager.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "GameMode/Combat/BattleState.h"

void UBattleTeamAIPlanner::Initialize(TArray<AActor*>& AllCombatants)
{
	Combatants = AllCombatants;

	QueryService = NewObject<UBattleAIQueryService>(this);
	if (QueryService)
	{
		QueryService->Initialize(AllCombatants);
	}

	ActionExecutor = NewObject<UBattleAIActionExecutor>(this);
	if (ActionExecutor)
	{
		ActionExecutor->Initialize();
		ActionExecutor->OnActionExecutionFinished.AddUObject(this, &UBattleTeamAIPlanner::OnActionExecutionFinished);
	}

	for (AActor* Actor : Combatants)
	{
		ACharacterBase* Unit = Cast<ACharacterBase>(Actor);
		if (!Unit)
		{
			continue;
		}

		UBattleAIComponent* AIComponent = Unit->FindComponentByClass<UBattleAIComponent>();
		if (!AIComponent)
		{
			AIComponent = NewObject<UBattleAIComponent>(Unit, TEXT("BattleAIComponent"));
			if (AIComponent)
			{
				AIComponent->RegisterComponent();
			}
		}

		if (AIComponent)
		{
			AIComponent->Initialize(QueryService);
		}
	}
}

void UBattleTeamAIPlanner::StartTeamTurn()
{
	if (bIsExecutingAction)
	{
		return;
	}

	TurnContext = FTeamTurnContext();
	EvaluateNextAction();
}

void UBattleTeamAIPlanner::SetBattleState(UBattleState* InBattleState)
{
	if (QueryService)
	{
		QueryService->SetBattleState(InBattleState);
	}
}

void UBattleTeamAIPlanner::Debug_LogCandidates(TArray<FTeamCandidateAction> Candidates)
{
	UE_LOG(LogTemp, Warning, TEXT("[BattleTeamAIPlanner] Candidates generated: %d"), Candidates.Num());

	for (const FTeamCandidateAction& Candidate : Candidates)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BattleTeamAIPlanner] Candidate | Unit=%s Type=%d Score=%.2f AP=%d Dmg=%.2f Target=%s Tile=(%d,%d) Reason=%s"),
			*GetNameSafe(Candidate.ActingUnit),
			static_cast<int32>(Candidate.ActionType),
			Candidate.Score,
			Candidate.ExpectedAPCost,
			Candidate.ExpectedDamage,
			*GetNameSafe(Candidate.Targeting.TargetActor),
			Candidate.Targeting.TargetTile.X,
			Candidate.Targeting.TargetTile.Y,
			*Candidate.Reason
		);
	}
}

void UBattleTeamAIPlanner::EvaluateNextAction()
{
	if (bIsExecutingAction)
	{
		return;
	}

	BuildTurnContext();
	ClearInvalidReservations();
	UpdateFocusTarget();

	TArray<FTeamCandidateAction> Candidates;
	GatherCandidateActions(Candidates);

	Debug_LogCandidates(Candidates);

	FTeamCandidateAction BestAction;
	if (!ChooseBestAction(Candidates, BestAction))
	{
		FinishTeamTurn();
		return;
	}

	if (BestAction.ActionType == ETeamAIActionType::EndTurn)
	{
		FinishTeamTurn();
		return;
	}

	ReserveActionConsequences(BestAction);
	bIsExecutingAction = true;

	UE_LOG(LogTemp, Warning, TEXT("[BattleTeamAIPlanner] Chosen action: Unit=%s Type=%d Score=%.2f Reason=%s"),
		*GetNameSafe(BestAction.ActingUnit),
		static_cast<int32>(BestAction.ActionType),
		BestAction.Score,
		*BestAction.Reason);

	if (!ActionExecutor || !ActionExecutor->ExecuteAction(BestAction))
	{
		bIsExecutingAction = false;
		FinishTeamTurn();
	}
}

void UBattleTeamAIPlanner::OnActionExecutionFinished(bool bSuccess)
{
	if (!bIsExecutingAction)
	{
		return;
	}

	bIsExecutingAction = false;

	if (!bSuccess)
	{
		FinishTeamTurn();
		return;
	}

	// Notify CombatManager so it can wait for camera and apply any post-action delay
	// before we evaluate the next action. CombatManager calls ContinueAfterActionDelay()
	// when it's ready.
	OnAIActionFinished.Broadcast();
}

void UBattleTeamAIPlanner::ContinueAfterActionDelay()
{
	EvaluateNextAction();
}

void UBattleTeamAIPlanner::FinishTeamTurn()
{
	bIsExecutingAction = false;
	TurnContext.bTurnShouldEnd = true;
	
	UE_LOG(LogTemp, Warning, TEXT("[BattleTeamAIPlanner] Finishing AI team turn"));

	if (UCombatManager* CombatManager = Cast<UCombatManager>(GetOuter()))
	{
		CombatManager->RequestEndEnemyTurn();
	}
}

void UBattleTeamAIPlanner::BuildTurnContext()
{
	if (!QueryService)
	{
		return;
	}

	TurnContext.AlliedUnits = QueryService->GetAlliedUnitsForTeam(EBattleUnitTeam::Enemy);
	TurnContext.EnemyUnits = QueryService->GetEnemyUnitsForTeam(EBattleUnitTeam::Enemy);

	TurnContext.AllCombatants.Reset();
	for (ACharacterBase* Unit : TurnContext.AlliedUnits)
	{
		if (Unit)
		{
			TurnContext.AllCombatants.Add(Unit);
		}
	}

	for (ACharacterBase* Unit : TurnContext.EnemyUnits)
	{
		if (Unit)
		{
			TurnContext.AllCombatants.Add(Unit);
		}
	}
}

void UBattleTeamAIPlanner::UpdateFocusTarget()
{
	TurnContext.FocusTarget = nullptr;
	float LowestHP = TNumericLimits<float>::Max();

	UBattleState* BattleStateRef = QueryService ? QueryService->GetBattleState() : nullptr;

	for (ACharacterBase* EnemyUnit : TurnContext.EnemyUnits)
	{
		if (!EnemyUnit) { continue; }

		// Read HP from BattleUnitState — it is the authority during combat
		if (BattleStateRef)
		{
			const FBattleUnitState* State = BattleStateRef->FindUnitState(EnemyUnit);
			if (!State || !State->IsAlive()) { continue; }

			if (State->CurrentHP < LowestHP)
			{
				LowestHP = State->CurrentHP;
				TurnContext.FocusTarget = EnemyUnit;
			}
		}
	}
}



void UBattleTeamAIPlanner::GatherCandidateActions(TArray<FTeamCandidateAction>& OutCandidates)
{
	for (ACharacterBase* AlliedUnit : TurnContext.AlliedUnits)
	{
		if (!AlliedUnit)
		{
			continue;
		}

		if (UBattleAIComponent* AIComponent = AlliedUnit->FindComponentByClass<UBattleAIComponent>())
		{
			AIComponent->GatherCandidateActions(TurnContext, OutCandidates);
		}
	}
}

bool UBattleTeamAIPlanner::ChooseBestAction(const TArray<FTeamCandidateAction>& Candidates, FTeamCandidateAction& OutBestAction) const
{
	if (Candidates.Num() == 0)
	{
		return false;
	}

	const FTeamCandidateAction* BestCandidate = nullptr;
	for (const FTeamCandidateAction& Candidate : Candidates)
	{
		if (!BestCandidate || Candidate.Score > BestCandidate->Score)
		{
			BestCandidate = &Candidate;
		}
	}

	if (!BestCandidate)
	{
		return false;
	}

	OutBestAction = *BestCandidate;
	return true;
}

void UBattleTeamAIPlanner::ReserveActionConsequences(const FTeamCandidateAction& Action)
{
	if (Action.Targeting.TargetActor && Action.ExpectedDamage > 0.f)
	{
		float& ReservedDamage = TurnContext.ReservedDamageByTarget.FindOrAdd(Action.Targeting.TargetActor);
		ReservedDamage += Action.ExpectedDamage;
	}

	if (Action.Targeting.bHasTileTarget)
	{
		TurnContext.ReservedTiles.Add(Action.Targeting.TargetTile);
	}
}

void UBattleTeamAIPlanner::ClearInvalidReservations()
{
	if (Combatants.Num() > 0)
	{
		ACharacterBase* AnyCharacter = Cast<ACharacterBase>(Combatants[0]);
		if (AnyCharacter && AnyCharacter->GetWorld())
		{
			if (UGridManager* Grid = AnyCharacter->GetWorld()->GetSubsystem<UGridManager>())
			{
				for (auto It = TurnContext.ReservedTiles.CreateIterator(); It; ++It)
				{
					if (!Grid->IsValidCoord(*It))
					{
						It.RemoveCurrent();
					}
				}
			}
		}
	}

	UBattleState* BattleStateRef = QueryService ? QueryService->GetBattleState() : nullptr;

	for (auto It = TurnContext.ReservedDamageByTarget.CreateIterator(); It; ++It)
	{
		ACharacterBase* Target = It.Key();

		const bool bDead = !Target
			|| !BattleStateRef
			|| [&]() {
				const FBattleUnitState* S = BattleStateRef->FindUnitState(Target);
				return !S || !S->IsAlive();
		}();

		if (bDead) { It.RemoveCurrent(); }
	}

}