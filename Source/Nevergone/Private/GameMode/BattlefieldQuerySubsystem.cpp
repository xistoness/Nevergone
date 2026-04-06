// Copyright Xyzto Works

#include "GameMode/BattlefieldQuerySubsystem.h"

#include "Actors/BattlefieldSpawnZone.h"
#include "Algo/RandomShuffle.h"
#include "EngineUtils.h"
#include "Level/GridManager.h"
#include "Types/LevelTypes.h"

void UBattlefieldQuerySubsystem::BuildCache()
{
	CachedCoverProviders.Reset();
	GetCoverProviders(CachedCoverProviders);
	bCacheValid = true;
}

void UBattlefieldQuerySubsystem::InvalidateCache()
{
	bCacheValid = false;
	CachedCoverProviders.Reset();
}

// ---------------------------------------------------------------------------
// Spawn zone queries
// ---------------------------------------------------------------------------

void UBattlefieldQuerySubsystem::GetSpawnZones(
	ESpawnZoneTeam Team,
	TArray<ABattlefieldSpawnZone*>& OutZones) const
{
	OutZones.Reset();
	for (TActorIterator<ABattlefieldSpawnZone> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && (*It)->Team == Team)
		{
			OutZones.Add(*It);
		}
	}
}

bool UBattlefieldQuerySubsystem::CollectValidTilesAroundZones(
	const UGridManager* Grid,
	const TArray<ABattlefieldSpawnZone*>& Zones,
	int32 RequiredCount,
	int32 MaxExpandedRadius,
	TArray<FIntPoint>& OutTiles) const
{
	OutTiles.Reset();

	if (!Grid || Zones.Num() == 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[BattlefieldQuerySubsystem] CollectValidTilesAroundZones: Grid=%s Zones=%d — aborting"),
			Grid ? TEXT("valid") : TEXT("null"), Zones.Num());
		return false;
	}

	// Start at the smallest SpawnRadiusTiles defined across the zones so we
	// always respect per-zone intent before expanding.
	int32 CurrentRadius = MAX_int32;
	for (const ABattlefieldSpawnZone* Zone : Zones)
	{
		if (Zone)
		{
			CurrentRadius = FMath::Min(CurrentRadius, Zone->SpawnRadiusTiles);
		}
	}
	if (CurrentRadius == MAX_int32) { CurrentRadius = 3; }

	// Expand radius until we have enough tiles or hit the hard cap.
	while (CurrentRadius <= MaxExpandedRadius)
	{
		OutTiles.Reset();
		TSet<FIntPoint> Visited;

		for (const ABattlefieldSpawnZone* Zone : Zones)
		{
			if (!Zone) { continue; }

			const FIntPoint ZoneCoord = Grid->WorldToGrid(Zone->GetActorLocation());

			if (!Grid->IsValidCoord(ZoneCoord))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[BattlefieldQuerySubsystem] Zone '%s' maps to invalid grid coord (%d,%d) — skipping"),
					*GetNameSafe(Zone), ZoneCoord.X, ZoneCoord.Y);
				continue;
			}

			CollectTilesInRadius(Grid, ZoneCoord, CurrentRadius, Visited, OutTiles);
		}

		if (OutTiles.Num() >= RequiredCount)
		{
			Algo::RandomShuffle(OutTiles);
			UE_LOG(LogTemp, Log,
				TEXT("[BattlefieldQuerySubsystem] Collected %d valid tiles (radius=%d, required=%d)"),
				OutTiles.Num(), CurrentRadius, RequiredCount);
			return true;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[BattlefieldQuerySubsystem] Only %d tiles at radius=%d, need %d — expanding"),
			OutTiles.Num(), CurrentRadius, RequiredCount);

		++CurrentRadius;
	}

	// Exhausted expansion budget — return whatever is available.
	Algo::RandomShuffle(OutTiles);
	UE_LOG(LogTemp, Error,
		TEXT("[BattlefieldQuerySubsystem] Could not collect %d tiles even at MaxRadius=%d — got %d"),
		RequiredCount, MaxExpandedRadius, OutTiles.Num());

	return OutTiles.Num() >= RequiredCount;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UBattlefieldQuerySubsystem::CollectTilesInRadius(
	const UGridManager* Grid,
	const FIntPoint& Center,
	int32 Radius,
	TSet<FIntPoint>& Visited,
	TArray<FIntPoint>& OutTiles) const
{
	for (int32 DX = -Radius; DX <= Radius; ++DX)
	{
		for (int32 DY = -Radius; DY <= Radius; ++DY)
		{
			const FIntPoint Coord(Center.X + DX, Center.Y + DY);

			if (Visited.Contains(Coord)) { continue; }
			Visited.Add(Coord);

			if (!Grid->IsValidCoord(Coord)) { continue; }

			const FGridTile* Tile = Grid->GetTile(Coord);
			if (!Tile || Tile->bBlocked) { continue; }

			// Reject tiles already occupied by a static world actor.
			if (Grid->GetActorAt(Coord)) { continue; }

			OutTiles.Add(Coord);
		}
	}
}

// ---------------------------------------------------------------------------
// Stubs
// ---------------------------------------------------------------------------

void UBattlefieldQuerySubsystem::GetCoverProviders(TArray<TWeakObjectPtr<AActor>>& OutCovers) const {}
bool UBattlefieldQuerySubsystem::HasCoverAtLocation(const FVector&) const { return false; }
bool UBattlefieldQuerySubsystem::HasLineOfEffect(const FVector&, const FVector&, const TArray<AActor*>&) const { return false; }
void UBattlefieldQuerySubsystem::QueryActorsInRadius(const FVector&, float, TArray<AActor*>&) const {}
void UBattlefieldQuerySubsystem::QueryActorsInCone(const FVector&, const FVector&, float, float, TArray<AActor*>&) const {}