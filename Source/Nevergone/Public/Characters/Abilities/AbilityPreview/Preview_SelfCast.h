// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/AbilityPreview/AbilityPreviewRenderer.h"
#include "Preview_SelfCast.generated.h"

class UGridManager;

/**
 * Preview renderer for self-cast abilities (GA_SelfCast).
 *
 * Highlights only the caster's own tile — no cursor targeting, no radius.
 * Uses SourceGridCoord instead of HoveredGridCoord because self-cast abilities
 * have no external target: the effect always resolves on the caster.
 *
 * Color convention (matches the rest of the preview system):
 *   White  — ability is available (AP sufficient, not on cooldown)
 *   Red    — ability is unavailable
 */
UCLASS()
class NEVERGONE_API UPreview_SelfCast : public UAbilityPreviewRenderer
{
	GENERATED_BODY()

public:
	virtual void UpdatePreview(const FActionContext& Context) override;

private:
	// Draws a box highlight on the caster's tile.
	void DrawCasterTile(const FIntPoint& GridCoord, UGridManager* Grid, bool bIsValid);
};