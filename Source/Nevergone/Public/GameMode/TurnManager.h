// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/BattleInputContext.h"
#include "UObject/Object.h"
#include "TurnManager.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTurnStateChanged, EBattleTurnOwner, EBattleTurnPhase);

UCLASS()
class NEVERGONE_API UTurnManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const TArray<AActor*>& InCombatants);
	
	void StartCombat();
	void EndCurrentTurn();

	EBattleTurnOwner GetCurrentTurnOwner() const;
	EBattleTurnPhase GetCurrentTurnPhase() const;

	FOnTurnStateChanged OnTurnStateChanged;

private:
	void BeginPlayerTurn();
	void BeginEnemyTurn();

	void SetTurnState(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase);
	
	UPROPERTY()
	TArray<TObjectPtr<AActor>> Combatants;

	EBattleTurnOwner CurrentTurnOwner = EBattleTurnOwner::Player;
	EBattleTurnPhase CurrentTurnPhase = EBattleTurnPhase::AwaitingOrders;
	
	int32 CurrentTurnIndex = INDEX_NONE;	
};
