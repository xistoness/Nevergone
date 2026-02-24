// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "ActorSaveData.generated.h"

USTRUCT(BlueprintType)
struct FSavePayload
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TArray<uint8> Data;
};

USTRUCT(BlueprintType)
struct FActorSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FGuid ActorGuid;

	UPROPERTY(SaveGame)
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(SaveGame)
	FTransform Transform;

	UPROPERTY(SaveGame)
	FName LevelName;

	// Custom data per save participant
	UPROPERTY(SaveGame)
	TMap<FName, FSavePayload> CustomDataMap;

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
