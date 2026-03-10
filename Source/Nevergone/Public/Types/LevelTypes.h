// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "LevelTypes.generated.h"

struct FGridTile
{
	FIntPoint GridCoord;      // (X, Y)
	FVector WorldLocation;    // World center
	float Height = 0.f;
	EGridSurfaceType SurfaceType;
	bool bBlocked = false;
	int32 MoveCost = 1;
	uint8 Flags;
	TArray<FIntPoint> Neighbors;
	TWeakObjectPtr<AActor> Occupant;
};

UENUM(BlueprintType)
enum class EGridSurfaceType : uint8
{
	Normal    UMETA(DisplayName = "Normal"),
};

USTRUCT(BlueprintType)
struct FLevelTransitionContext
{
	GENERATED_BODY()

	UPROPERTY()
	FName FromLevel;

	UPROPERTY()
	FName ToLevel;

	UPROPERTY()
	FName EntryPortalID;
};