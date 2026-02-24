// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/AbilityPreview/AbilityPreviewRenderer.h"
#include "Preview_Move.generated.h"

struct FActionContext;
class UGridManager;

UCLASS()
class NEVERGONE_API UPreview_Move : public UAbilityPreviewRenderer
{
	GENERATED_BODY()
	
	virtual void Initialize(UWorld* InWorld) override;
	
	virtual void UpdatePreview(const FActionContext& Context) override;
	
	void DrawPath(const TArray<FIntPoint>& Path, UGridManager* Grid);
	
	void DrawInvalidPath(const TArray<FIntPoint>& Path, UGridManager* Grid);
	
};
