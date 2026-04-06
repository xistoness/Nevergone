// Copyright Xyzto Works

#include "Characters/Abilities/AbilityPreview/Preview_SelfCast.h"

#include "Level/GridManager.h"
#include "Types/BattleTypes.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"

void UPreview_SelfCast::UpdatePreview(const FActionContext& Context)
{
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Preview_SelfCast] UpdatePreview: World is invalid"));
		return;
	}

	UGridManager* Grid = World->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Preview_SelfCast] UpdatePreview: GridManager not found"));
		return;
	}

	// Self-cast always affects the caster — highlight the source tile, not the hovered tile.
	DrawCasterTile(Context.SourceGridCoord, Grid, Context.bIsActionValid);
}

void UPreview_SelfCast::DrawCasterTile(const FIntPoint& GridCoord, UGridManager* Grid, bool bIsValid)
{
	if (!Grid)
	{
		return;
	}

	const FVector Location = Grid->GridToWorld(GridCoord);

	// White when available, red when not — distinct from attack (yellow) and AoE (green)
	// so the player can immediately read "this targets me" at a glance.
	DrawDebugBox(
		World,
		Location + FVector(0.f, 0.f, 10.f),
		FVector(44.f, 44.f, 12.f),
		bIsValid ? FColor::White : FColor::Red,
		false,
		0.f,
		0,
		3.f
	);
}