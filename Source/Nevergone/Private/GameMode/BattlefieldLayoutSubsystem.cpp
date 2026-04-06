// Copyright Xyzto Works

#include "GameMode/BattlefieldLayoutSubsystem.h"

#include "Actors/BattlefieldSpawnZone.h"
#include "Characters/CharacterBase.h"
#include "GameMode/BattlePreparationContext.h"
#include "GameMode/BattlefieldQuerySubsystem.h"
#include "Level/GridManager.h"

void UBattlefieldLayoutSubsystem::PlanSpawns(
	UBattlefieldQuerySubsystem& BattlefieldQuery,
	UGridManager& Grid,
	UBattlePreparationContext& BattlePrepContext)
{
	BattlePrepContext.EnemyPlannedSpawns.Reset();
	BattlePrepContext.PlayerPlannedSpawns.Reset();

	PlanTeamSpawns(BattlefieldQuery, Grid, BattlePrepContext, ESpawnZoneTeam::Enemy);
	PlanTeamSpawns(BattlefieldQuery, Grid, BattlePrepContext, ESpawnZoneTeam::Ally);
}

void UBattlefieldLayoutSubsystem::PlanTeamSpawns(
	UBattlefieldQuerySubsystem& BattlefieldQuery,
	UGridManager& Grid,
	UBattlePreparationContext& BattlePrepContext,
	ESpawnZoneTeam Team)
{
	const bool bIsEnemy = (Team == ESpawnZoneTeam::Enemy);

	// Select the correct party array based on team.
	const int32 RequiredCount = bIsEnemy
		? BattlePrepContext.EnemyParty.Num()
		: BattlePrepContext.PlayerParty.Num();

	if (RequiredCount == 0) { return; }

	// 1. Gather spawn zones for this team.
	TArray<ABattlefieldSpawnZone*> Zones;
	BattlefieldQuery.GetSpawnZones(Team, Zones);

	if (Zones.Num() == 0)
	{
		// GameContextManager guards against missing enemy zones before calling
		// PlanSpawns. This log covers the ally case and acts as a secondary guard.
		UE_LOG(LogTemp, Error,
			TEXT("[BattlefieldLayoutSubsystem] PlanTeamSpawns: no spawn zones found for team %s — spawns skipped"),
			bIsEnemy ? TEXT("Enemy") : TEXT("Ally"));
		return;
	}

	// 2. Collect candidate tiles, expanding radius if the pool is too small.
	TArray<FIntPoint> CandidateTiles;
	const bool bEnoughTiles = BattlefieldQuery.CollectValidTilesAroundZones(
		&Grid,
		Zones,
		RequiredCount,
		MaxSpawnRadiusExpansion,
		CandidateTiles
	);

	if (!bEnoughTiles)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BattlefieldLayoutSubsystem] PlanTeamSpawns (%s): only %d valid tiles for %d units — some units will not spawn"),
			bIsEnemy ? TEXT("Enemy") : TEXT("Ally"), CandidateTiles.Num(), RequiredCount);
	}

	// 3. Assign one tile per unit. CandidateTiles is already shuffled.
	const int32 SpawnCount = FMath::Min(CandidateTiles.Num(), RequiredCount);

	for (int32 i = 0; i < SpawnCount; ++i)
	{
		const FVector TileWorld = Grid.GetTileCenterWorld(CandidateTiles[i]);
		const FTransform SpawnTransform(FRotator::ZeroRotator, TileWorld);

		FPlannedSpawn Spawn;
		Spawn.PlannedTransform = SpawnTransform;

		if (bIsEnemy)
		{
			Spawn.ActorClass = BattlePrepContext.EnemyParty[i].EnemyClass;
			BattlePrepContext.EnemyPlannedSpawns.Add(Spawn);
		}
		else
		{
			Spawn.ActorClass = BattlePrepContext.PlayerParty[i].PlayerClass;
			BattlePrepContext.PlayerPlannedSpawns.Add(Spawn);
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("[BattlefieldLayoutSubsystem] PlanTeamSpawns (%s): planned %d/%d spawns"),
		bIsEnemy ? TEXT("Enemy") : TEXT("Ally"), SpawnCount, RequiredCount);
}