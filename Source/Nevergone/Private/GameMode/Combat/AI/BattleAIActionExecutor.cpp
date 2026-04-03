// Copyright Xyzto Works

#include "GameMode/Combat/AI/BattleAIActionExecutor.h"

#include "ActorComponents/BattleModeComponent.h"
#include "Level/GridManager.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "GameMode/Combat/BattleState.h"

void UBattleAIActionExecutor::Initialize()
{
	CleanupPendingBindings();
}

bool UBattleAIActionExecutor::ExecuteAction(const FTeamCandidateAction& Action)
{
	CleanupPendingBindings();

	ActiveUnit = Action.ActingUnit;
	if (!ActiveUnit)
	{
		OnActionExecutionFinished.Broadcast(false);
		return false;
	}

	UBattleModeComponent* BattleMode = ActiveUnit->GetBattleModeComponent();
	if (!BattleMode)
	{
		OnActionExecutionFinished.Broadcast(false);
		return false;
	}

	if (Action.ActionType == ETeamAIActionType::EndTurn)
	{
		OnActionExecutionFinished.Broadcast(true);
		return true;
	}

	BattleMode->OnActionUseFinished.AddDynamic(this, &UBattleAIActionExecutor::HandleActionFinished);

	FActionContext Context;
	Context.SourceActor = ActiveUnit;
	Context.AbilityClass = Action.AbilityClass;
	Context.AbilityDefinition = Action.AbilityDefinition;
	Context.SourceWorldPosition = ActiveUnit->GetActorLocation();
	Context.SelectionPhase = EBattleActionSelectionPhase::SelectingTarget;

	if (Action.ActionType == ETeamAIActionType::Move)
	{
		Context.HoveredGridCoord = Action.Targeting.TargetTile;
		Context.MovementTargetGridCoord = Action.Targeting.TargetTile;
		if (ActiveUnit->GetWorld())
		{
			if (UGridManager* Grid = ActiveUnit->GetWorld()->GetSubsystem<UGridManager>())
			{
				Context.SourceGridCoord = Grid->WorldToGrid(ActiveUnit->GetActorLocation());
				Context.HoveredWorldPosition = Grid->GridToWorld(Action.Targeting.TargetTile);
				Context.MovementTargetWorldPosition = Context.HoveredWorldPosition;
				Context.TargetActor = Grid->GetActorAt(Action.Targeting.TargetTile);

				UBattleState* BattleStateLocal   = BattleMode ? BattleMode->GetBattleState() : nullptr;
				const FBattleUnitState* UnitState = BattleStateLocal
					? BattleStateLocal->FindUnitState(ActiveUnit)
					: nullptr;

				if (UnitState)
				{
					Context.TraversalParams  = UnitState->TraversalParams;
					Context.CachedPathCost   = Grid->CalculatePathCost(
						Context.SourceGridCoord,
						Context.HoveredGridCoord,   // or SelectedApproachGridCoord where applicable
						Context.TraversalParams
					);
				}
			}
		}
	}
	else if (Action.ActionType == ETeamAIActionType::UseAbility)
	{
		Context.TargetActor = Action.Targeting.TargetActor;
		if (ActiveUnit->GetWorld())
		{
			if (UGridManager* Grid = ActiveUnit->GetWorld()->GetSubsystem<UGridManager>())
			{
				Context.SourceGridCoord = Grid->WorldToGrid(ActiveUnit->GetActorLocation());
				if (Action.Targeting.TargetActor)
				{
					Context.LockedTargetActor = Action.Targeting.TargetActor;
					Context.TargetActor = Action.Targeting.TargetActor;
					Context.LockedTargetGridCoord = Grid->WorldToGrid(Action.Targeting.TargetActor->GetActorLocation());
					Context.HoveredGridCoord = Context.LockedTargetGridCoord;
					Context.HoveredWorldPosition = Grid->GridToWorld(Context.HoveredGridCoord);
					Context.bHasLockedTarget = true;
				}

				if (Action.Targeting.bHasTileTarget)
				{
					Context.SelectionPhase = EBattleActionSelectionPhase::SelectingApproachTile;
					Context.SelectedApproachGridCoord = Action.Targeting.TargetTile;
					Context.bHasSelectedApproachTile = true;
					Context.HoveredGridCoord = Action.Targeting.TargetTile;
					Context.HoveredWorldPosition = Grid->GridToWorld(Action.Targeting.TargetTile);

					UBattleState* BattleStateLocal   = BattleMode ? BattleMode->GetBattleState() : nullptr;
					const FBattleUnitState* UnitState = BattleStateLocal
						? BattleStateLocal->FindUnitState(ActiveUnit)
						: nullptr;

					if (UnitState)
					{
						Context.TraversalParams  = UnitState->TraversalParams;
						Context.CachedPathCost   = Grid->CalculatePathCost(
							Context.SourceGridCoord,
							Context.HoveredGridCoord,
							Context.TraversalParams
						);
					}
				}
			}
		}
	}

	const bool bStarted = BattleMode->ExecuteActionFromAI(Context);
	if (!bStarted)
	{
		CleanupPendingBindings();
		OnActionExecutionFinished.Broadcast(false);
	}

	return bStarted;
}

void UBattleAIActionExecutor::CleanupPendingBindings()
{
	if (ActiveUnit)
	{
		if (UBattleModeComponent* BattleMode = ActiveUnit->GetBattleModeComponent())
		{
			BattleMode->OnActionUseFinished.RemoveAll(this);
		}
	}

	ActiveUnit = nullptr;
}

void UBattleAIActionExecutor::HandleActionFinished(ACharacterBase* ActingUnit)
{
	if (!ActiveUnit)
	{
		return;
	}

	// Ignore finish from another unit
	if (ActingUnit && ActingUnit != ActiveUnit)
	{
		return;
	}

	CleanupPendingBindings();
	OnActionExecutionFinished.Broadcast(true);
}