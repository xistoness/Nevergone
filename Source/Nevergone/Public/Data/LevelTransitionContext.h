// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "LevelTransitionContext.generated.h"

USTRUCT(BlueprintType)
struct FLevelTransitionContext
{
	GENERATED_BODY()

	UPROPERTY()
	FName FromLevel;

	UPROPERTY()
	FName ToLevel;

	UPROPERTY()
	FName EntryPointTag;
};
