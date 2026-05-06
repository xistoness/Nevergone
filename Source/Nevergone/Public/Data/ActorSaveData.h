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

// ---------------------------------------------------------------------------
// FActorSaveData
//
// One entry per SaveableActor captured during CollectWorldSaveData.
//
// ActorIdentifier is the stable key used to match a save entry to a live actor:
//   - Placed actors:  the actor's FName (serialized in the level .uasset by the engine).
//   - Spawned actors: a stringified GUID generated at runtime by SaveableComponent.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FActorSaveData
{
	GENERATED_BODY()

public:

	// Stable identifier for this actor across save/load cycles.
	// For placed actors: the actor FName (e.g. BP_Sword_C_UAID_047C16C9...).
	// For spawned actors: a runtime-generated GUID string.
	UPROPERTY(SaveGame)
	FName ActorIdentifier;

	UPROPERTY(SaveGame)
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(SaveGame)
	FTransform Transform;

	UPROPERTY(SaveGame)
	FName LevelName;

	// Mirrors USaveableComponent::bPersistAcrossLoads at save time.
	// SpawnMissingActors reads this instead of the CDO, because instance-level
	// overrides (e.g. dropped items set true at runtime) are not reflected on the CDO.
	UPROPERTY(SaveGame)
	bool bPersistAcrossLoads = false;

	// Custom data per save participant (item state, encounter state, etc.)
	UPROPERTY(SaveGame)
	TMap<FName, FSavePayload> CustomDataMap;

	FActorSaveData()
		: ActorClass(nullptr)
		, Transform(FTransform::Identity)
		, bPersistAcrossLoads(false)
	{
	}

	bool IsValid() const
	{
		return !ActorIdentifier.IsNone();
	}
};

// ---------------------------------------------------------------------------
// FLevelSaveData
//
// Persisted state for one level (one entry per level in MySaveGame).
// ---------------------------------------------------------------------------
USTRUCT()
struct FLevelSaveData
{
	GENERATED_BODY()

	// Snapshot of all saveable actors captured during CollectWorldSaveData.
	// On subsequent visits, RestoreSaveableActors diffs this against the live world.
	UPROPERTY(SaveGame)
	TArray<FActorSaveData> Actors;

	// True after the first-visit snapshot has been taken for this level.
	// While false, RestoreWorldState skips the diff/restore entirely and only
	// takes a baseline snapshot of the original level state.
	UPROPERTY(SaveGame)
	bool bHasBeenVisited = false;
};
