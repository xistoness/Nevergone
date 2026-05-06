// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldManagerSubsystem.generated.h"

class UMyGameInstance;
class AActor;
struct FActorSaveData;


UCLASS()
class NEVERGONE_API UWorldManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	// Subsystem lifecycle
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void HandleWorldInitializedActors(const FActorsInitializedParams& Params);

	// Collects current world state into the active save slot.
	// Called by AutoSaveBeforeTravel via OnSaveRequested.
	void CollectWorldSaveData();

	// Entry point for world restoration after a level load.
	// On first visit: takes a snapshot and returns early (no diff).
	// On subsequent visits: diffs saved state against live world.
	UFUNCTION(BlueprintCallable, Category = "Save")
	void RestoreWorldState();

	FName GetSanitizedLevelKey() const;

private:

	// GameInstance delegate handlers
	void HandleSaveLoaded();
	void HandlePartyRestored(const struct FPartyData& PartyData);

	// Restore pipeline steps — only run on subsequent visits (bHasBeenVisited = true)
	void RestoreSaveableActors();
	void SpawnMissingActors();
	void ResolveActorConflicts();
	void CallPostRestore();

	void ClearCachedData();

	UPROPERTY()
	UMyGameInstance* GameInstance;

	// Guards against HandleSaveLoaded being called twice when streaming sublevels
	// trigger a second HandleWorldInitializedActors on the same world load.
	bool bDidRestoreMidCombat = false;
};
