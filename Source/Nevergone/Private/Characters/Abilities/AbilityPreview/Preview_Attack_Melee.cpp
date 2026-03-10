// Copyright Xyzto Works

#include "Characters/Abilities/AbilityPreview/Preview_Attack_Melee.h"

#include "Level/GridManager.h"
#include "Types/BattleTypes.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"

void UPreview_Attack_Melee::UpdatePreview(const FActionContext& Context)
{
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Preview_Attack_Melee]: World is invalid..."));
		return;
	}

	UGridManager* Grid = World->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Preview_Attack_Melee]: Grid is invalid..."));
		return;
	}

	// Draw approach path only if we actually have a resolved movement destination
	if (Context.MovementTargetGridCoord != FIntPoint::ZeroValue)
	{
		TArray<FIntPoint> Path;
		Grid->FindPath(
			Context.SourceGridCoord,
			Context.MovementTargetGridCoord,
			Context.TraversalParams,
			Path
		);
		
		// Draw last tile from approach
		const FVector MoveLocation = Grid->GridToWorld(Context.MovementTargetGridCoord);
		DrawDebugSphere(
			World,
			MoveLocation + FVector(0.f, 0.f, 20.f),
			24.f,
			10,
			Context.bIsActionValid ? FColor::Green : FColor::Red,
			false,
			0.f
		);

		// If the approach tile is the same as source tile, path may be empty or trivial. That's okay.
		if (Context.bIsActionValid)
		{
			DrawPath(Path, Grid);
		}
		else
		{
			DrawInvalidPath(Path, Grid);
		}
	}

	// Draw hovered tile as the real attack target tile
	DrawAttackTile(Context.HoveredGridCoord, Grid, Context.bIsActionValid);
}

void UPreview_Attack_Melee::DrawAttackTile(const FIntPoint& GridCoord, UGridManager* Grid, bool bIsValid)
{
	if (!Grid)
	{
		return;
	}

	const FVector Location = Grid->GridToWorld(GridCoord);

	DrawDebugBox(
		World,
		Location + FVector(0.f, 0.f, 10.f),
		FVector(40.f, 40.f, 10.f),
		bIsValid ? FColor::Yellow : FColor::Red,
		false,
		0.f,
		0,
		2.5f
	);
}