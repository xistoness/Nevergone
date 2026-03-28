// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/AbilityPreview/AbilityPreviewRenderer.h"
#include "Preview_Attack_Ranged.generated.h"

class UGridManager;

UCLASS()
class NEVERGONE_API UPreview_Attack_Ranged : public UAbilityPreviewRenderer
{
	GENERATED_BODY()

public:
	virtual void UpdatePreview(const FActionContext& Context) override;

protected:
	// Draws the target tile highlight at the given grid coordinate
	void DrawTargetTile(const FIntPoint& GridCoord, UGridManager* Grid, bool bIsValid);

	// Draws a line from source to target to represent the ranged shot trajectory
	void DrawTrajectoryLine(const FVector& SourceLocation, const FVector& TargetLocation, bool bIsValid);
};