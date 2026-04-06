// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/BattleTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattlefieldLayoutSubsystem.generated.h"

enum class ESpawnZoneTeam : uint8;
class UBattlePreparationContext;
class UBattlefieldQuerySubsystem;
class UGridManager;

UCLASS()
class NEVERGONE_API UBattlefieldLayoutSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Plans spawn positions for both teams and populates
	 * BattlePrepContext.EnemyPlannedSpawns and PlayerPlannedSpawns.
	 *
	 * Both sides use the same zone-based tile system via
	 * BattlefieldQuerySubsystem::CollectValidTilesAroundZones(). The ally
	 * tile pool is also stored on BattlePrepContext so the future
	 * BattlePreparation placement UI can present it to the player.
	 *
	 * @param BattlefieldQuery  World subsystem that provides zone/tile data.
	 * @param Grid              Active GridManager for tile-to-world conversion.
	 * @param BattlePrepContext Output context to populate.
	 */
	void PlanSpawns(
		UBattlefieldQuerySubsystem& BattlefieldQuery,
		UGridManager& Grid,
		UBattlePreparationContext& BattlePrepContext);

private:
	void PlanTeamSpawns(
		UBattlefieldQuerySubsystem& BattlefieldQuery,
		UGridManager& Grid,
		UBattlePreparationContext& BattlePrepContext,
		ESpawnZoneTeam Team);

	/** Hard cap on radius expansion when the tile pool is too small. */
	static constexpr int32 MaxSpawnRadiusExpansion = 10;
};