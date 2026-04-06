// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Actors/BattlefieldSpawnZone.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattlefieldQuerySubsystem.generated.h"

class AActor;
class UGridManager;

UCLASS()
class NEVERGONE_API UBattlefieldQuerySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Called before combat preparation — rebuilds all caches from world scan. */
	void BuildCache();

	/** Clears all cached data. */
	void InvalidateCache();

	// -----------------------------------------------------------------------
	// Spawn zone queries
	// -----------------------------------------------------------------------

	/**
	 * Returns all ABattlefieldSpawnZone actors in the world that match Team.
	 * Enemy and ally zones share the same base class; the team is set via the
	 * enum in each Blueprint CDO so designers never configure it per actor.
	 */
	void GetSpawnZones(ESpawnZoneTeam Team, TArray<ABattlefieldSpawnZone*>& OutZones) const;

	/**
	 * Collects valid (non-blocked, unoccupied) grid tiles within
	 * SpawnRadiusTiles of each zone center.
	 *
	 * Starts at the smallest SpawnRadiusTiles across the provided zones and
	 * expands one tile at a time until RequiredCount tiles are found or
	 * MaxExpandedRadius is reached.
	 *
	 * @param Grid              Active GridManager for this battle.
	 * @param Zones             Zones from GetSpawnZones().
	 * @param RequiredCount     Minimum number of tiles needed.
	 * @param MaxExpandedRadius Hard cap on radius expansion.
	 * @param OutTiles          Output: shuffled candidate tile coords.
	 * @return                  True if OutTiles.Num() >= RequiredCount.
	 */
	bool CollectValidTilesAroundZones(
		const UGridManager* Grid,
		const TArray<ABattlefieldSpawnZone*>& Zones,
		int32 RequiredCount,
		int32 MaxExpandedRadius,
		TArray<FIntPoint>& OutTiles) const;

	// -----------------------------------------------------------------------
	// Other queries (stubs for future use)
	// -----------------------------------------------------------------------
	void GetCoverProviders(TArray<TWeakObjectPtr<AActor>>& OutCovers) const;
	bool HasCoverAtLocation(const FVector& Location) const;
	bool HasLineOfEffect(const FVector& From, const FVector& To, const TArray<AActor*>& IgnoredActors) const;
	void QueryActorsInRadius(const FVector& Origin, float Radius, TArray<AActor*>& OutActors) const;
	void QueryActorsInCone(const FVector& Origin, const FVector& Forward, float AngleDegrees, float Distance, TArray<AActor*>& OutActors) const;

private:
	/** Appends unblocked, unoccupied tiles within Radius of Center. Skips already-visited coords. */
	void CollectTilesInRadius(
		const UGridManager* Grid,
		const FIntPoint& Center,
		int32 Radius,
		TSet<FIntPoint>& Visited,
		TArray<FIntPoint>& OutTiles) const;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> CachedCoverProviders;

	bool bCacheValid = false;
};