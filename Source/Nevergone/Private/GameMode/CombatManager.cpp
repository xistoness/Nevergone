// Copyright Xyzto Works


#include "GameMode/CombatManager.h"
#include "GameMode/BattlePreparationContext.h"
#include "GameMode/BattlefieldLayoutSubsystem.h"
#include "GameMode/TurnManager.h"
#include "Types/BattleTypes.h"

void UCombatManager::Initialize()
{
	// Setup participants
	// Setup tactical grid usage
}

void UCombatManager::EnterPreparation(UBattlePreparationContext& BattlePrepContext)
{
	// Lock exploration input
	// Switch camera mode
	// Initialize preview systems
}

void UCombatManager::StartCombat(UBattlePreparationContext& BattlePrepContext)
{	
	// Spawn allies
	for (const FPlannedSpawn& Spawn : BattlePrepContext.PlayerPlannedSpawns)
	{
		AActor* SpawnedAlly = GetWorld()->SpawnActor<AActor>(
			Spawn.ActorClass,
			Spawn.PlannedTransform
		);
		
		if (SpawnedAlly)
		{
			SpawnedAllies.Add(SpawnedAlly);
		}
	}
	
	// Spawn enemies
	for (const FPlannedSpawn& Spawn : BattlePrepContext.EnemyPlannedSpawns)
	{
		AActor* SpawnedEnemy = GetWorld()->SpawnActor<AActor>(
			Spawn.ActorClass,
			Spawn.PlannedTransform
		);
		
		if (SpawnedEnemy)
		{
			SpawnedEnemies.Add(SpawnedEnemy);
		}
	}
	TArray<AActor*> AllCombatants;
	AllCombatants.Append(SpawnedAllies);
	AllCombatants.Append(SpawnedEnemies);

	TurnManager = NewObject<UTurnManager>(this);
	TurnManager->Initialize(AllCombatants);
	TurnManager->StartTurns();
}

void UCombatManager::CancelPreparation()
{
	Cleanup();
}

void UCombatManager::EndCombat()
{
	Cleanup();
	OnCombatFinished.Broadcast();
}

void UCombatManager::Cleanup()
{
	if (TurnManager)
	{
		TurnManager = nullptr;
	}

	// Clear references
	SpawnedEnemies.Empty();
}