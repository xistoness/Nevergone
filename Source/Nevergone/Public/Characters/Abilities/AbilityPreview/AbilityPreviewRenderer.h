// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AbilityPreviewRenderer.generated.h"

struct FActionContext;

UCLASS(Abstract, Blueprintable)
class NEVERGONE_API UAbilityPreviewRenderer : public UObject
{
	GENERATED_BODY()
	
public:

	virtual void Initialize(UWorld* InWorld);

	virtual void UpdatePreview(const FActionContext& Context);

	virtual void ClearPreview();

protected:

	UPROPERTY()
	UWorld* World;
};
