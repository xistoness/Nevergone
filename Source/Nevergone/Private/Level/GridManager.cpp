// Copyright Xyzto Works


#include "Level/GridManager.h"
#include "Level/BattleVolume.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"


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

void UGridManager::GenerateGrid(const ABattleVolume* BattleVolume, const UWorld* World)
{
	if (!BattleVolume)
	{
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[GridManager]: Starting grid generation"));
	
	GridTiles.Empty();

	Origin     = BattleVolume->GetOrigin();
	TileSize   = BattleVolume->GetTileSize();
	GridHeight = BattleVolume->GetGridHeight();

	const FVector Extent = BattleVolume->GetExtent();

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

			const float WorldX = Origin.X - Extent.X + (X + 0.5f) * TileSize;
			const float WorldY = Origin.Y - Extent.Y + (Y + 0.5f) * TileSize;

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
				Tile.WorldLocation = Hit.Location;
				Tile.Height = Hit.Location.Z;
				Tile.bBlocked = false;
				Tile.MoveCost = 1;
			}
			else
			{
				Tile.WorldLocation = FVector(WorldX, WorldY, Origin.Z);
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
	
	InitializeOccupancy(BattleVolume);
}

void UGridManager::InitializeOccupancy(const ABattleVolume* BattleVolume)
{
	if (!BattleVolume)
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	const FBox VolumeBounds = BattleVolume->GetComponentsBoundingBox();

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
	// Convert world location into local space relative to grid origin
	const FVector Local = WorldLocation - Origin;

	const int32 X = FMath::RoundToInt((Local.X + (GridWidth * TileSize * 0.5f)) / TileSize);
	const int32 Y = FMath::RoundToInt((Local.Y + (GridHeightCount * TileSize * 0.5f)) / TileSize);

	return FIntPoint(X, Y);
}

FVector UGridManager::GridToWorld(const FIntPoint& Coord) const
{
	const float WorldX = Origin.X - (GridWidth * TileSize * 0.5f) + (Coord.X + 0.5f) * TileSize;
	const float WorldY = Origin.Y - (GridHeightCount * TileSize * 0.5f) + (Coord.Y + 0.5f) * TileSize;

	return FVector(WorldX, WorldY, Origin.Z);
}

bool UGridManager::IsValidCoord(const FIntPoint& Coord) const
{
	return Coord.X >= 0 && Coord.X < GridWidth
		&& Coord.Y >= 0 && Coord.Y < GridHeightCount;
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

int32 UGridManager::CalculatePathCost(const FIntPoint& Start, const FIntPoint& Goal) const
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

int32 UGridManager::Heuristic(const FIntPoint& A, const FIntPoint& B) const
{
	const int32 DX = FMath::Abs(A.X - B.X);
	const int32 DY = FMath::Abs(A.Y - B.Y);

	return FMath::Max(DX, DY);
}

bool UGridManager::FindPath(const FIntPoint& Start, const FIntPoint& Goal, TArray<FIntPoint>& OutPath) const
{
	OutPath.Reset();

	const FGridTile* StartTile = GetTile(Start);
	const FGridTile* GoalTile  = GetTile(Goal);

	if (!StartTile || !GoalTile || GoalTile->bBlocked)
	{
		return false;
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
		// Find node with lower F
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

		// Got to destination 
		if (CurrentCoord == Goal)
		{
			// Rebuild path
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
			
			AActor* Occupant = GetActorAt(NeighborCoord);

			if (Occupant && NeighborCoord != Goal)
			{
				continue;
			}

			const int32 TentativeG =
				AllNodes[CurrentCoord].GCost + NeighborTile->MoveCost;

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
	UE_LOG(LogTemp, Warning, TEXT("[GridManager]: Couldn't find a valid path..."));
	return false;
}