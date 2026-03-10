// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/AbilityPreview/Preview_Move.h"
#include "Preview_Attack_Melee.generated.h"

class UGridManager;

UCLASS()
class NEVERGONE_API UPreview_Attack_Melee : public UPreview_Move
{
	GENERATED_BODY()

public:
	virtual void UpdatePreview(const FActionContext& Context) override;

protected:
	void DrawAttackTile(const FIntPoint& GridCoord, UGridManager* Grid, bool bIsValid);
};