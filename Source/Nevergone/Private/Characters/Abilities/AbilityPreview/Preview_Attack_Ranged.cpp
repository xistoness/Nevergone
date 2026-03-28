// Copyright Xyzto Works

#include "Characters/Abilities/AbilityPreview/Preview_Attack_Ranged.h"

#include "Level/GridManager.h"
#include "Types/BattleTypes.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"

void UPreview_Attack_Ranged::UpdatePreview(const FActionContext& Context)
{
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Preview_Attack_Ranged]: World is invalid..."));
		return;
	}

	UGridManager* Grid = World->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Preview_Attack_Ranged]: Grid is invalid..."));
		return;
	}

	// Draw the hovered tile as the attack target
	DrawTargetTile(Context.HoveredGridCoord, Grid, Context.bIsActionValid);

	// Draw trajectory line from source to hovered target
	DrawTrajectoryLine(Context.SourceWorldPosition, Context.HoveredWorldPosition, Context.bIsActionValid);
}

void UPreview_Attack_Ranged::DrawTargetTile(const FIntPoint& GridCoord, UGridManager* Grid, bool bIsValid)
{
	if (!Grid)
	{
		return;
	}

	const FVector Location = Grid->GridToWorld(GridCoord);

	// Outer box to highlight the target tile
	DrawDebugBox(
		World,
		Location + FVector(0.f, 0.f, 10.f),
		FVector(40.f, 40.f, 10.f),
		bIsValid ? FColor::Cyan : FColor::Red,
		false,
		0.f,
		0,
		2.5f
	);

	// Center dot to make the target more visible
	DrawDebugSphere(
		World,
		Location + FVector(0.f, 0.f, 20.f),
		16.f,
		8,
		bIsValid ? FColor::Cyan : FColor::Red,
		false,
		0.f
	);
}

void UPreview_Attack_Ranged::DrawTrajectoryLine(
	const FVector& SourceLocation,
	const FVector& TargetLocation,
	bool bIsValid
)
{
	// Offset slightly upward so the line floats above the ground
	const FVector OffsetZ = FVector(0.f, 0.f, 40.f);

	DrawDebugLine(
		World,
		SourceLocation + OffsetZ,
		TargetLocation + OffsetZ,
		bIsValid ? FColor::Cyan : FColor::Red,
		false,
		0.f,
		0,
		1.5f
	);
}