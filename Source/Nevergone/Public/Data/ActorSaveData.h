// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "ActorSaveData.generated.h"

USTRUCT(BlueprintType)
struct FActorSaveData
{
	GENERATED_BODY()

public:
	// Persistent unique identifier for this actor
	UPROPERTY(SaveGame)
	FGuid ActorGuid;

	// Class used to respawn this actor if missing
	UPROPERTY(SaveGame)
	TSoftClassPtr<AActor> ActorClass;

	// World transform at save time
	UPROPERTY(SaveGame)
	FTransform Transform;

	// Level 
	UPROPERTY(SaveGame)
	FName LevelName;
	
	// Optional custom binary payload (actor-defined)
	UPROPERTY(SaveGame)
	TArray<uint8> CustomData;

	FActorSaveData()
		: ActorGuid()
		, ActorClass(nullptr)
		, Transform(FTransform::Identity)
	{
	}

	bool IsValid() const
	{
		return ActorGuid.IsValid();
	}
};
