// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/AbilityPreview/AbilityPreviewRenderer.h"
#include "Preview_AOE.generated.h"

class UGridManager;

UCLASS()
class NEVERGONE_API UPreview_AOE : public UAbilityPreviewRenderer
{
	GENERATED_BODY()

public:
	virtual void UpdatePreview(const FActionContext& Context) override;

	// Radius of the AOE highlight in grid tiles — must match the ability's HealRadius
	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	int32 Radius = 1;

protected:
	// Draws the center tile (cast target) highlight
	void DrawCenterTile(const FIntPoint& GridCoord, UGridManager* Grid, bool bIsValid);

	// Draws all tiles inside the AOE radius, excluding the center
	void DrawRadiusTiles(const FIntPoint& CenterCoord, UGridManager* Grid, bool bIsValid);
};