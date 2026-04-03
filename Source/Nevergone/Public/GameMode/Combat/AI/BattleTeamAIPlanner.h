// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/AIBattleTypes.h"
#include "BattleTeamAIPlanner.generated.h"

// Fired when an AI action finishes and the planner is ready for the next step.
// CombatManager listens to this to insert camera-wait logic before calling EvaluateNextAction.
DECLARE_MULTICAST_DELEGATE(FOnAIActionFinished);

class ACharacterBase;
class UBattleAIActionExecutor;
class UBattleAIQueryService;
class UBattleState;

UCLASS()
class NEVERGONE_API UBattleTeamAIPlanner : public UObject
{
	GENERATED_BODY()
	
public:
	void Initialize(TArray<AActor*>& AllCombatants);
	void StartTeamTurn();
	void SetBattleState(UBattleState* InBattleState);

	// Called by CombatManager after it finishes its post-action work (camera wait, delay).
	void ContinueAfterActionDelay();

	FOnAIActionFinished OnAIActionFinished;
	
protected:
	void EvaluateNextAction();
	void OnActionExecutionFinished(bool bSuccess);
	void FinishTeamTurn();
	
private:
	void BuildTurnContext();
	void UpdateFocusTarget();
	void GatherCandidateActions(TArray<FTeamCandidateAction>& OutCandidates);
	bool ChooseBestAction(const TArray<FTeamCandidateAction>& Candidates, FTeamCandidateAction& OutBestAction) const;
	void ReserveActionConsequences(const FTeamCandidateAction& Action);
	void ClearInvalidReservations();
	
	void Debug_LogCandidates(TArray<FTeamCandidateAction> Candidates);

private:
	UPROPERTY()
	TArray<TObjectPtr<AActor>> Combatants;

	UPROPERTY()
	UBattleAIQueryService* QueryService = nullptr;

	UPROPERTY()
	UBattleAIActionExecutor* ActionExecutor = nullptr;

	UPROPERTY()
	FTeamTurnContext TurnContext;

	UPROPERTY()
	bool bIsExecutingAction = false;
};