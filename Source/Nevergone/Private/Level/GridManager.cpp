// Copyright Xyzto Works


#include "Level/GridManager.h"
#include "Level/FloorEncounterVolume.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Types/BattleTypes.h"


struct FAStarNode
{
	FIntPoint Coord;

	int32 GCost = 0; // cost from start to this point
	int32 HCost = 0; // heuristics to destination

	FIntPoint Parent;

	int32 FCost() const
	{
		return GCost + HCost;
	}
};

void UGridManager::GenerateGrid(const AFloorEncounterVolume* EncounterVolume, const UWorld* World)
{
	if (!EncounterVolume)
	{
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[GridManager]: Starting grid generation"));
	
	GridTiles.Empty();

	Origin     = EncounterVolume->GetBattleBoxOrigin();
	TileSize   = EncounterVolume->GetTileSize();
	GridHeight = EncounterVolume->GetGridHeight();

	const FVector Extent = EncounterVolume->GetBattleBoxExtent();

	GridWidth       = FMath::FloorToInt((Extent.X * 2) / TileSize);
	GridHeightCount = FMath::FloorToInt((Extent.Y * 2) / TileSize);

	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GridManager]: World is not valid!"));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[GridManager]: GenerateGrid gets to loop"));
	for (int32 X = 0; X < GridWidth; ++X)
	{
		for (int32 Y = 0; Y < GridHeightCount; ++Y)
		{
			FGridTile Tile;
			Tile.GridCoord = FIntPoint(X, Y);

			const float WorldX = GetGridMinX() + (X + 0.5f) * TileSize;
			const float WorldY = GetGridMinY() + (Y + 0.5f) * TileSize;

			const FVector TraceStart(WorldX, WorldY, Origin.Z + GridHeight);
			const FVector TraceEnd  (WorldX, WorldY, Origin.Z - GridHeight);

			FHitResult Hit;
			FCollisionQueryParams Params;
			Params.bTraceComplex = false;

			const bool bHit = World->LineTraceSingleByChannel(
				Hit,
				TraceStart,
				TraceEnd,
				ECC_Visibility,
				Params
			);

			if (bHit)
			{
				Tile.Height = Hit.Location.Z;
				Tile.WorldLocation = FVector(WorldX, WorldY, Tile.Height);
				Tile.bBlocked = false;
				Tile.MoveCost = 1;
			}
			else
			{
				Tile.Height = Origin.Z;
				Tile.WorldLocation = FVector(WorldX, WorldY, Tile.Height);
				Tile.bBlocked = true;
			}

			GridTiles.Add(Tile.GridCoord, Tile);
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[GridManager]: GenerateGrid reached end"));
	BuildNeighborCache();
	UE_LOG(LogTemp, Warning, TEXT("[GridManager]: bDrawDebugTiles = %s"),
	bDrawDebugTiles ? TEXT("TRUE") : TEXT("FALSE"));
	if (bDrawDebugTiles)
	{
		DrawDebugGrid(World, 10.f);
	}
	
	InitializeOccupancy(EncounterVolume);
}

void UGridManager::InitializeOccupancy(const AFloorEncounterVolume* EncounterVolume)
{
	if (!EncounterVolume)
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	// Use only the BattleBox bounds, not the full actor bounds (which would
	// include TriggerBox and SkeletalMesh and produce a larger, incorrect area)
	const FVector BoxOrigin = EncounterVolume->GetBattleBoxOrigin();
	const FVector BoxExtent = EncounterVolume->GetBattleBoxExtent();
	const FBox VolumeBounds(BoxOrigin - BoxExtent, BoxOrigin + BoxExtent);

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;

		if (!IsValid(Actor))
			continue;

		if (VolumeBounds.IsInside(Actor->GetActorLocation()))
		{
			RegisterActorToGrid(Actor);
		}
	}
}

void UGridManager::RegisterActorToGrid(AActor* Actor)
{
	if (!Actor)
		return;

	const FIntPoint Coord = WorldToGrid(Actor->GetActorLocation());

	if (!IsValidCoord(Coord))
		return;

	OccupancyMap.Add(Coord, Actor);
}

void UGridManager::UpdateActorPosition(AActor* Actor, const FIntPoint& Coord)
{
	if (!Actor)
		return;
	
	if (const FIntPoint* OldCoord = OccupancyMap.FindKey(Actor))
	{
		OccupancyMap.Remove(*OldCoord);
	}

	if (IsValidCoord(Coord))
	{
		OccupancyMap.Add(Coord, Actor);
	}
}

float UGridManager::GetGridMinX() const
{
	return Origin.X - (GridWidth * TileSize * 0.5f);
}

float UGridManager::GetGridMinY() const
{
	return Origin.Y - (GridHeightCount * TileSize * 0.5f);
}

const FGridTile* UGridManager::GetTile(const FIntPoint& Coord) const
{
	if (!IsValidCoord(Coord))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GridManager]: Tile is invalid at: %s"), *Coord.ToString());
		return nullptr;
	}
	return GridTiles.Find(Coord);
}

FIntPoint UGridManager::WorldToGrid(const FVector& WorldLocation) const
{
	const int32 X = FMath::FloorToInt((WorldLocation.X - GetGridMinX()) / TileSize);
	const int32 Y = FMath::FloorToInt((WorldLocation.Y - GetGridMinY()) / TileSize);

	return FIntPoint(X, Y);
}

FVector UGridManager::GridToWorld(const FIntPoint& Coord) const
{
	const FGridTile* Tile = GetTile(Coord);
	if (Tile)
	{
		return Tile->WorldLocation;
	}

	const float WorldX = GetGridMinX() + (Coord.X + 0.5f) * TileSize;
	const float WorldY = GetGridMinY() + (Coord.Y + 0.5f) * TileSize;

	return FVector(WorldX, WorldY, Origin.Z);
}

bool UGridManager::IsValidCoord(const FIntPoint& Coord) const
{
	return Coord.X >= 0 && Coord.X < GridWidth
		&& Coord.Y >= 0 && Coord.Y < GridHeightCount;
}

bool UGridManager::IsTraversalAllowed(const FGridTile& FromTile, const FGridTile& ToTile,
	const FGridTraversalParams& Params) const
{
	if (ToTile.bBlocked)
	{
		return false;
	}

	const float DeltaZ = ToTile.Height - FromTile.Height;

	if (DeltaZ > Params.MaxStepUpHeight)
	{
		return false;
	}

	if (-DeltaZ > Params.MaxStepDownHeight)
	{
		return false;
	}

	return true;
}

void UGridManager::BuildNeighborCache()
{
	static const FIntPoint Directions[] = {
		{ 1,  0}, {-1,  0}, { 0,  1}, { 0, -1}, // cardinals
		{ 1,  1}, { 1, -1}, {-1,  1}, {-1, -1}  // diagonals
	};

	for (auto& Pair : GridTiles)
	{
		FGridTile& Tile = Pair.Value;
		Tile.Neighbors.Reset();

		for (const FIntPoint& Dir : Directions)
		{
			const FIntPoint NeighborCoord = Tile.GridCoord + Dir;

			if (!IsValidCoord(NeighborCoord))
			{
				continue;
			}

			const FGridTile* NeighborTile = GetTile(NeighborCoord);
			if (!NeighborTile || NeighborTile->bBlocked)
			{
				continue;
			}

			// Diagonal movement
			if (Dir.X != 0 && Dir.Y != 0)
			{
				const FIntPoint Adjacent1 = Tile.GridCoord + FIntPoint(Dir.X, 0);
				const FIntPoint Adjacent2 = Tile.GridCoord + FIntPoint(0, Dir.Y);

				const FGridTile* Tile1 = GetTile(Adjacent1);
				const FGridTile* Tile2 = GetTile(Adjacent2);

				if (!Tile1 || !Tile2)
				{
					continue; // corner cutting
				}
				
				if (Tile1->bBlocked && Tile2->bBlocked)
				{
					continue;
				}
			}

			Tile.Neighbors.Add(NeighborCoord);
		}
	}
}

int32 UGridManager::CalculatePathCost(const FIntPoint& Start, const FIntPoint& Goal, const FGridTraversalParams& TraversalParams) const
{
	const FGridTile* StartTile = GetTile(Start);
    const FGridTile* GoalTile  = GetTile(Goal);

    if (!StartTile || !GoalTile || GoalTile->bBlocked)
    {
        return INDEX_NONE;
    }

    TMap<FIntPoint, FAStarNode> AllNodes;
    TSet<FIntPoint> OpenSet;
    TSet<FIntPoint> ClosedSet;

    FAStarNode StartNode;
    StartNode.Coord = Start;
    StartNode.GCost = 0;
    StartNode.HCost = Heuristic(Start, Goal);

    AllNodes.Add(Start, StartNode);
    OpenSet.Add(Start);

    while (OpenSet.Num() > 0)
    {
        // Find node with lowest F cost
        FIntPoint CurrentCoord;
        int32 LowestF = TNumericLimits<int32>::Max();

        for (const FIntPoint& Coord : OpenSet)
        {
            const FAStarNode& Node = AllNodes[Coord];
            if (Node.FCost() < LowestF)
            {
                LowestF = Node.FCost();
                CurrentCoord = Coord;
            }
        }

        // If arrived at goal → return cost
        if (CurrentCoord == Goal)
        {
            return AllNodes[CurrentCoord].GCost;
        }

        OpenSet.Remove(CurrentCoord);
        ClosedSet.Add(CurrentCoord);

        const FGridTile* CurrentTile = GetTile(CurrentCoord);
        if (!CurrentTile)
        {
            continue;
        }

        for (const FIntPoint& NeighborCoord : CurrentTile->Neighbors)
        {
            if (ClosedSet.Contains(NeighborCoord))
            {
                continue;
            }

            const FGridTile* NeighborTile = GetTile(NeighborCoord);
            if (!NeighborTile || NeighborTile->bBlocked)
            {
                continue;
            }

            const int32 TentativeG =
                AllNodes[CurrentCoord].GCost + NeighborTile->MoveCost;

            FAStarNode* NeighborNode =
                AllNodes.Find(NeighborCoord);

            if (!NeighborNode)
            {
                FAStarNode NewNode;
                NewNode.Coord  = NeighborCoord;
                NewNode.GCost  = TentativeG;
                NewNode.HCost  = Heuristic(NeighborCoord, Goal);
                NewNode.Parent = CurrentCoord;

                AllNodes.Add(NeighborCoord, NewNode);
                OpenSet.Add(NeighborCoord);
            }
            else if (TentativeG < NeighborNode->GCost)
            {
                NeighborNode->GCost  = TentativeG;
                NeighborNode->Parent = CurrentCoord;
                OpenSet.Add(NeighborCoord);
            }
        }
    }

    return INDEX_NONE; // no path
}

AActor* UGridManager::GetActorAt(const FIntPoint& Coord) const
{
	if (const TWeakObjectPtr<AActor>* Found = OccupancyMap.Find(Coord))
	{
		return Found->Get();
	}

	return nullptr;
}

void UGridManager::DrawDebugGrid(const UWorld* World, float Duration) const
{
	UE_LOG(LogTemp, Warning, TEXT("[GridManager]: Trying to DrawDebugGrid..."));
#if WITH_EDITOR
	if (!World)
	{
		return;
	}

	for (const auto& Pair : GridTiles)
	{
		const FGridTile& Tile = Pair.Value;

		const FColor Color =
			Tile.bBlocked ? FColor::Red : FColor::Green;

		DrawDebugBox(
			World,
			Tile.WorldLocation,
			FVector(TileSize * 0.5f, TileSize * 0.5f, 5.f),
			Color,
			false,
			Duration,
			0,
			1.f
		);

		// Optional: draw grid coordinates
		DrawDebugString(
			World,
			Tile.WorldLocation + FVector(0.f, 0.f, 15.f),
			FString::Printf(TEXT("(%d,%d)"),
				Tile.GridCoord.X,
				Tile.GridCoord.Y),
			nullptr,
			FColor::White,
			Duration
		);
	}
#endif
}

void UGridManager::RemoveActorFromGrid(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// Look up by actor pointer, not by recalculating world-to-grid from current location.
	// Recalculation is unreliable: the actor may have moved slightly (Z snap, physics
	// adjustment) between registration and removal, causing WorldToGrid to return a
	// different coordinate than what was stored, leaving the tile permanently occupied.
	if (const FIntPoint* RegisteredCoord = OccupancyMap.FindKey(Actor))
	{
		UE_LOG(LogTemp, Log,
			TEXT("[GridManager] RemoveActorFromGrid: removed %s from tile (%d, %d)"),
			*GetNameSafe(Actor), RegisteredCoord->X, RegisteredCoord->Y);
		OccupancyMap.Remove(*RegisteredCoord);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GridManager] RemoveActorFromGrid: %s not found in OccupancyMap — already removed or never registered"),
			*GetNameSafe(Actor));
	}
}

bool UGridManager::FindClosestValidTileToWorld(
	const FVector& WorldLocation,
	FIntPoint& OutCoord,
	bool bRequireUnoccupied
) const
{
	bool bFoundAny = false;
	float BestDistSq = TNumericLimits<float>::Max();
	FIntPoint BestCoord = FIntPoint::ZeroValue;

	for (const TPair<FIntPoint, FGridTile>& Pair : GridTiles)
	{
		const FGridTile& Tile = Pair.Value;

		if (Tile.bBlocked)
		{
			continue;
		}

		if (bRequireUnoccupied && GetActorAt(Tile.GridCoord))
		{
			continue;
		}

		const FVector TileCenter = GetTileCenterWorld(Tile.GridCoord);
		const float DistSq = FVector::DistSquared2D(WorldLocation, TileCenter);

		if (!bFoundAny || DistSq < BestDistSq)
		{
			bFoundAny = true;
			BestDistSq = DistSq;
			BestCoord = Tile.GridCoord;
		}
	}

	if (!bFoundAny)
	{
		return false;
	}

	OutCoord = BestCoord;
	return true;
}

FVector UGridManager::GetTileCenterWorld(const FIntPoint& Coord) const
{
	const FGridTile* Tile = GetTile(Coord);
	if (!Tile)
	{
		return GridToWorld(Coord);
	}

	const float GridMinX = Origin.X - (GridWidth * TileSize * 0.5f);
	const float GridMinY = Origin.Y - (GridHeightCount * TileSize * 0.5f);

	const float WorldX = GridMinX + (Coord.X + 0.5f) * TileSize;
	const float WorldY = GridMinY + (Coord.Y + 0.5f) * TileSize;

	return FVector(WorldX, WorldY, Tile->WorldLocation.Z);
}

int32 UGridManager::Heuristic(const FIntPoint& A, const FIntPoint& B) const
{
	const int32 DX = FMath::Abs(A.X - B.X);
	const int32 DY = FMath::Abs(A.Y - B.Y);

	return FMath::Max(DX, DY);
}

bool UGridManager::FindPath(
	const FIntPoint& Start,
	const FIntPoint& Goal,
	const FGridTraversalParams& TraversalParams,
	TArray<FIntPoint>& OutPath
) const
{
	OutPath.Reset();

	const FGridTile* StartTile = GetTile(Start);
	const FGridTile* GoalTile = GetTile(Goal);

	if (!StartTile || !GoalTile || GoalTile->bBlocked)
	{
		return false;
	}

	static const FIntPoint Directions[] = {
		{ 1,  0}, {-1,  0}, { 0,  1}, { 0, -1},
		{ 1,  1}, { 1, -1}, {-1,  1}, {-1, -1}
	};

	TMap<FIntPoint, FAStarNode> AllNodes;
	TSet<FIntPoint> OpenSet;
	TSet<FIntPoint> ClosedSet;

	FAStarNode StartNode;
	StartNode.Coord = Start;
	StartNode.GCost = 0;
	StartNode.HCost = Heuristic(Start, Goal);

	AllNodes.Add(Start, StartNode);
	OpenSet.Add(Start);

	while (OpenSet.Num() > 0)
	{
		FIntPoint CurrentCoord;
		int32 LowestF = TNumericLimits<int32>::Max();

		for (const FIntPoint& Coord : OpenSet)
		{
			const FAStarNode& Node = AllNodes[Coord];
			if (Node.FCost() < LowestF)
			{
				LowestF = Node.FCost();
				CurrentCoord = Coord;
			}
		}

		if (CurrentCoord == Goal)
		{
			FIntPoint Trace = Goal;
			while (Trace != Start)
			{
				OutPath.Insert(Trace, 0);
				Trace = AllNodes[Trace].Parent;
			}
			OutPath.Insert(Start, 0);
			return true;
		}

		OpenSet.Remove(CurrentCoord);
		ClosedSet.Add(CurrentCoord);

		const FGridTile* CurrentTile = GetTile(CurrentCoord);
		if (!CurrentTile)
		{
			continue;
		}

		for (const FIntPoint& Dir : Directions)
		{
			const FIntPoint NeighborCoord = CurrentCoord + Dir;

			if (!IsValidCoord(NeighborCoord) || ClosedSet.Contains(NeighborCoord))
			{
				continue;
			}

			const FGridTile* NeighborTile = GetTile(NeighborCoord);
			if (!NeighborTile)
			{
				continue;
			}

			if (!IsTraversalAllowed(*CurrentTile, *NeighborTile, TraversalParams))
			{
				continue;
			}

			AActor* Occupant = GetActorAt(NeighborCoord);
			if (Occupant && NeighborCoord != Goal && !TraversalParams.bIgnoreOccupiedTiles)
			{
				continue;
			}

			// Diagonal validation
			if (Dir.X != 0 && Dir.Y != 0)
			{
				const FIntPoint Adjacent1 = CurrentCoord + FIntPoint(Dir.X, 0);
				const FIntPoint Adjacent2 = CurrentCoord + FIntPoint(0, Dir.Y);

				const FGridTile* Tile1 = GetTile(Adjacent1);
				const FGridTile* Tile2 = GetTile(Adjacent2);

				if (!Tile1 || !Tile2)
				{
					continue;
				}

				if (!IsTraversalAllowed(*CurrentTile, *Tile1, TraversalParams) &&
					!IsTraversalAllowed(*CurrentTile, *Tile2, TraversalParams))
				{
					continue;
				}
			}

			const int32 TentativeG = AllNodes[CurrentCoord].GCost + NeighborTile->MoveCost;

			FAStarNode* NeighborNode = AllNodes.Find(NeighborCoord);
			if (!NeighborNode)
			{
				FAStarNode NewNode;
				NewNode.Coord = NeighborCoord;
				NewNode.GCost = TentativeG;
				NewNode.HCost = Heuristic(NeighborCoord, Goal);
				NewNode.Parent = CurrentCoord;

				AllNodes.Add(NeighborCoord, NewNode);
				OpenSet.Add(NeighborCoord);
			}
			else if (TentativeG < NeighborNode->GCost)
			{
				NeighborNode->GCost = TentativeG;
				NeighborNode->Parent = CurrentCoord;
				OpenSet.Add(NeighborCoord);
			}
		}
	}

	return false;
}