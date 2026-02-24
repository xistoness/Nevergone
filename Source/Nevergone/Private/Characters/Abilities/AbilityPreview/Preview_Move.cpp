// Copyright Xyzto Works


#include "Characters/Abilities/AbilityPreview/Preview_Move.h"

#include "Level/GridManager.h"
#include "Types/BattleTypes.h"

#include "Engine/World.h"


void UPreview_Move::Initialize(UWorld* InWorld)
{
	Super::Initialize(InWorld);
}

void UPreview_Move::UpdatePreview(const FActionContext& Context)
{
	UE_LOG(LogTemp, Warning, TEXT("[Preview_Move]: Trying to update preview..."));
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Preview_Move]: World is invalid..."));
		return;
	}
		

	UGridManager* Grid = World->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Preview_Move]: Grid is invalid..."));
		return;
	}

	TArray<FIntPoint> Path;
	Grid->FindPath(Context.SourceGridCoord,
					   Context.TargetGridCoord,
					   Path);
	
	if (Context.bIsActionValid)
			DrawPath(Path, Grid);
	else
	{
		DrawInvalidPath(Path, Grid);
	}
}

void UPreview_Move::DrawPath(const TArray<FIntPoint>& Path, UGridManager* Grid)
{
	UE_LOG(LogTemp, Warning, TEXT("[Preview_Move]: Trying to draw path..."));
	for (const FIntPoint& Coord : Path)
	{
		const FVector Location =
			Grid->GridToWorld(Coord);

		DrawDebugSphere(
			World,
			Location,
			20.f,
			8,
			FColor::Blue,
			false,
			0.f
		);
	}
}

void UPreview_Move::DrawInvalidPath(const TArray<FIntPoint>& Path, UGridManager* Grid)
{
	UE_LOG(LogTemp, Warning, TEXT("[Preview_Move]: Trying to draw invalid path..."));
	for (const FIntPoint& Coord : Path)
	{
		const FVector Location =
			Grid->GridToWorld(Coord);

		DrawDebugSphere(
			World,
			Location,
			20.f,
			8,
			FColor::Red,
			false,
			0.f
		);
	}
}