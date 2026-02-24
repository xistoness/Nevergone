// Copyright Xyzto Works


#include "GameMode/TurnManager.h"

void UTurnManager::Initialize(const TArray<AActor*>& InCombatants)
{
	Combatants = InCombatants;
}

void UTurnManager::StartCombat()
{
	BeginPlayerTurn();
}

void UTurnManager::BeginPlayerTurn()
{
	UE_LOG(LogTemp, Warning, TEXT("[TurnManager] Starting Player turn"));
	SetTurnState(EBattleTurnOwner::Player, EBattleTurnPhase::AwaitingOrders);
}

void UTurnManager::BeginEnemyTurn()
{
	UE_LOG(LogTemp, Warning, TEXT("[TurnManager] Starting Enemy turn"));
	SetTurnState(EBattleTurnOwner::Enemy, EBattleTurnPhase::ExecutingActions);

	// Enemy AI execution would be triggered here
	// When finished, EndCurrentTurn() must be called
}

void UTurnManager::EndCurrentTurn()
{
	if (CurrentTurnOwner == EBattleTurnOwner::Player)
	{
		BeginEnemyTurn();
	}
	else
	{
		BeginPlayerTurn();
	}
}

void UTurnManager::SetTurnState(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase)
{
	CurrentTurnOwner = NewOwner;
	CurrentTurnPhase = NewPhase;

	OnTurnStateChanged.Broadcast(CurrentTurnOwner, CurrentTurnPhase);
}

EBattleTurnOwner UTurnManager::GetCurrentTurnOwner() const
{
	return CurrentTurnOwner;
}

EBattleTurnPhase UTurnManager::GetCurrentTurnPhase() const
{
	return CurrentTurnPhase;
}