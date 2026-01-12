// Copyright Xyzto Works


#include "GameMode/BattlefieldLayoutSubsystem.h"
#include "GameMode/BattlePreparationContext.h"
#include "GameMode/BattlefieldQuerySubsystem.h"
#include "Characters/CharacterBase.h"

#include "Algo/RandomShuffle.h"

void UBattlefieldLayoutSubsystem::PlanSpawns(UBattlefieldQuerySubsystem& BattlefieldQuery,
	UBattlePreparationContext& BattlePrepContext)
{
	BattlePrepContext.EnemyPlannedSpawns.Reset();
	BattlePrepContext.PlayerPlannedSpawns.Reset();
	PlanEnemySpawns(BattlefieldQuery, BattlePrepContext);
	PlanAllySpawns(BattlefieldQuery, BattlePrepContext);
}

void UBattlefieldLayoutSubsystem::PlanEnemySpawns(UBattlefieldQuerySubsystem& BattlefieldQuery,
                                                  UBattlePreparationContext& BattlePrepContext)
{
	TArray<FTransform> EnemySlots;
	BattlefieldQuery.GetEnemySpawnSlots(EnemySlots);

	// Shuffle for variety
	Algo::RandomShuffle(EnemySlots);

	const int32 Count = FMath::Min(
		EnemySlots.Num(),
		BattlePrepContext.EnemyParty.Num()
	);

	for (int32 i = 0; i < Count; ++i)
	{
		FPlannedSpawn Spawn;
		Spawn.ActorClass = BattlePrepContext.EnemyParty[i].EnemyClass;
		Spawn.PlannedTransform = EnemySlots[i];

		BattlePrepContext.EnemyPlannedSpawns.Add(Spawn);
	}
}

void UBattlefieldLayoutSubsystem::PlanAllySpawns(UBattlefieldQuerySubsystem& BattlefieldQuery,
	UBattlePreparationContext& BattlePrepContext)
{
	TArray<FTransform> PlayerSlots;
	BattlefieldQuery.GetPlayerSpawnSlots(PlayerSlots);

	const int32 Count = FMath::Min(
		PlayerSlots.Num(),
		BattlePrepContext.PlayerParty.Num()
	);

	for (int32 i = 0; i < Count; ++i)
	{
		FPlannedSpawn Spawn;
		Spawn.ActorClass = BattlePrepContext.PlayerParty[i].PlayerClass;
		Spawn.PlannedTransform = PlayerSlots[i];

		BattlePrepContext.PlayerPlannedSpawns.Add(Spawn);
	}	
}
