// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/LevelTypes.h"
#include "GridManager.generated.h"

class ABattleVolume;

UCLASS()
class NEVERGONE_API UGridManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void GenerateGrid(const ABattleVolume* BattleVolume, const UWorld* World);
	
	void InitializeOccupancy(const ABattleVolume* BattleVolume);
	
	void RegisterActorToGrid(AActor* Actor);
	
	void UpdateActorPosition(AActor* Actor, const FIntPoint& Coord);

	const FGridTile* GetTile(const FIntPoint& Coord) const;

	FIntPoint WorldToGrid(const FVector& WorldLocation) const;
	FVector GridToWorld(const FIntPoint& Coord) const;
	
	bool IsValidCoord(const FIntPoint& Coord) const;
	
	void BuildNeighborCache();
	
	int32 CalculatePathCost(const FIntPoint& Start,	const FIntPoint& Goal) const;
	
	AActor* GetActorAt(const FIntPoint& Coord) const;
	
	bool FindPath(const FIntPoint& Start, const FIntPoint& Goal, TArray<FIntPoint>& OutPath) const;
	
	UPROPERTY(EditAnywhere, Category="Debug")
	bool bDrawDebugTiles = true;
	
	void DrawDebugGrid(const UWorld* World, float Duration = 0.f) const;

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
