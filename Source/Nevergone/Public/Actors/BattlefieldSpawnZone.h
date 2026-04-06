// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattlefieldSpawnZone.generated.h"

UENUM(BlueprintType)
enum class ESpawnZoneTeam : uint8
{
	Enemy UMETA(DisplayName = "Enemy"),
	Ally  UMETA(DisplayName = "Ally"),
};

/**
 * Base class for battlefield spawn zones.
 *
 * Defines a center position and a tile radius. BattlefieldQuerySubsystem
 * collects valid (non-blocked, unoccupied) grid tiles within that radius
 * to build the spawn candidate pool for each team.
 *
 * Do NOT place this base class directly in levels. Instead create two
 * Blueprint children — one with Team = Enemy, one with Team = Ally —
 * and place those. The enum is set once in the Blueprint CDO so level
 * designers never need to configure it per actor.
 *
 * Radius expansion: if the combined candidate pool across all zones of a
 * team is smaller than the number of units that need to be placed,
 * BattlefieldQuerySubsystem automatically expands the radius until enough
 * tiles are found or the hard cap (MaxSpawnRadiusExpansion) is hit.
 */
UCLASS(Abstract)
class NEVERGONE_API ABattlefieldSpawnZone : public AActor
{
	GENERATED_BODY()

public:
	ABattlefieldSpawnZone();

	/** Which team this zone belongs to. Set this once in the Blueprint CDO. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Zone")
	ESpawnZoneTeam Team = ESpawnZoneTeam::Enemy;

	/**
	 * Radius in grid tiles around this zone's center from which valid spawn
	 * tiles are drawn. Expanded automatically when the pool is too small.
	 * Default: 3 for enemies, 2 for allies — set in the Blueprint CDO.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Zone", meta = (ClampMin = "1"))
	int32 SpawnRadiusTiles = 3;

protected:
	virtual auto BeginPlay() -> void override;
};