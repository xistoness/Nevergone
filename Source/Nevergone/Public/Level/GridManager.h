// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/LevelTypes.h"
#include "GridManager.generated.h"

struct FGridTraversalParams;
class AFloorEncounterVolume;

UCLASS()
class NEVERGONE_API UGridManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Generates the tactical grid from the encounter volume's BattleBox. */
	void GenerateGrid(const AFloorEncounterVolume* EncounterVolume, const UWorld* World);
	
	void InitializeOccupancy(const AFloorEncounterVolume* EncounterVolume);
	
	void RegisterActorToGrid(AActor* Actor);
	
	void UpdateActorPosition(AActor* Actor, const FIntPoint& Coord);
	float GetGridMinX() const;
	float GetGridMinY() const;

	const FGridTile* GetTile(const FIntPoint& Coord) const;

	FIntPoint WorldToGrid(const FVector& WorldLocation) const;
	FVector GridToWorld(const FIntPoint& Coord) const;
	
	bool IsValidCoord(const FIntPoint& Coord) const;
	
	bool IsTraversalAllowed(const FGridTile& FromTile, const FGridTile& ToTile, const FGridTraversalParams& Params) const;
	
	void BuildNeighborCache();
	
	int32 CalculatePathCost(const FIntPoint& Start,	const FIntPoint& Goal, const FGridTraversalParams& TraversalParams) const;
	
	AActor* GetActorAt(const FIntPoint& Coord) const;
	
	bool FindPath(const FIntPoint& Start, const FIntPoint& Goal, const FGridTraversalParams& TraversalParams, TArray<FIntPoint>& OutPath) const;
	
	UPROPERTY(EditAnywhere, Category="Debug")
	bool bDrawDebugTiles = true;
	
	void DrawDebugGrid(const UWorld* World, float Duration = 0.f) const;
	
	void RemoveActorFromGrid(AActor* Actor);
	
	bool FindClosestValidTileToWorld(const FVector& WorldLocation, FIntPoint& OutCoord, bool bRequireUnoccupied = true) const;

	FVector GetTileCenterWorld(const FIntPoint& Coord) const;

private:
	
	int32 Heuristic(const FIntPoint& A, const FIntPoint& B) const;
	
	TMap<FIntPoint, FGridTile> GridTiles;
	TMap<FIntPoint, TWeakObjectPtr<AActor>> OccupancyMap;

	FVector Origin;
	float TileSize = 100.f;
	float GridHeight = 500.f;

	int32 GridWidth = 0;
	int32 GridHeightCount = 0;
	
	
};