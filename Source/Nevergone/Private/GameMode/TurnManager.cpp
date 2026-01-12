// Copyright Xyzto Works


#include "GameMode/TurnManager.h"

void UTurnManager::Initialize(const TArray<AActor*>& InCombatants)
{
	Combatants.Reset();

	for (AActor* Actor : InCombatants)
	{
		if (Actor)
		{
			Combatants.Add(Actor);
		}
	}

	CurrentTurnIndex = INDEX_NONE;
}

void UTurnManager::StartTurns()
{
	if (Combatants.Num() == 0)
	{
		return;
	}

	CurrentTurnIndex = 0;
	OnTurnAdvanced.Broadcast();
}

void UTurnManager::AdvanceTurn()
{
	if (Combatants.Num() == 0)
	{
		return;
	}

	CurrentTurnIndex = (CurrentTurnIndex + 1) % Combatants.Num();
	OnTurnAdvanced.Broadcast();
}