// Copyright Xyzto Works

#include "Characters/Abilities/AbilityPreview/Preview_AOE.h"

#include "Level/GridManager.h"
#include "Types/BattleTypes.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"

void UPreview_AOE::UpdatePreview(const FActionContext& Context)
{
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Preview_AOE]: World is invalid..."));
		return;
	}

	UGridManager* Grid = World->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Preview_AOE]: Grid is invalid..."));
		return;
	}

	// Draw the hovered tile as the AOE center
	DrawCenterTile(Context.HoveredGridCoord, Grid, Context.bIsActionValid);

	// Draw all tiles within the AOE radius
	DrawRadiusTiles(Context.HoveredGridCoord, Grid, Context.bIsActionValid);
}

void UPreview_AOE::DrawCenterTile(const FIntPoint& GridCoord, UGridManager* Grid, bool bIsValid)
{
	if (!Grid)
	{
		return;
	}

	const FVector Location = Grid->GridToWorld(GridCoord);

	// Slightly larger box to distinguish the center from the radius tiles
	DrawDebugBox(
		World,
		Location + FVector(0.f, 0.f, 10.f),
		FVector(44.f, 44.f, 12.f),
		bIsValid ? FColor::Green : FColor::Red,
		false,
		0.f,
		0,
		3.f
	);
}

void UPreview_AOE::DrawRadiusTiles(const FIntPoint& CenterCoord, UGridManager* Grid, bool bIsValid)
{
	if (!Grid)
	{
		return;
	}

	for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
	{
		for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
		{
			// Skip the center tile — already drawn by DrawCenterTile
			if (OffsetX == 0 && OffsetY == 0)
			{
				continue;
			}

			const FIntPoint TileCoord(CenterCoord.X + OffsetX, CenterCoord.Y + OffsetY);

			if (!Grid->IsValidCoord(TileCoord))
			{
				continue;
			}

			const FVector Location = Grid->GridToWorld(TileCoord);

			DrawDebugBox(
				World,
				Location + FVector(0.f, 0.f, 10.f),
				FVector(40.f, 40.f, 10.f),
				bIsValid ? FColor::Green : FColor::Red,
				false,
				0.f,
				0,
				1.5f
			);
		}
	}
}