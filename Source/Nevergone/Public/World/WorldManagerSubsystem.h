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
	void HandleWorldInitializedActors(const FActorsInitializedParams& Params);
	virtual void Deinitialize() override;
	
	void CollectWorldSaveData();
	
	// Single public entry point used after level load
	UFUNCTION(BlueprintCallable, Category="Save")
	void RestoreWorldState();

private:

	// GameInstance listeners
	void HandleSaveLoaded();
	void HandlePartyRestored(const struct FPartyData& PartyData);

	// Core responsibilities
	void RestoreSaveableActors();
	void SpawnMissingActors();
	void ResolveActorConflicts();
	void CallPostRestore();

	// Utilities
	void ClearCachedData();
	
	UPROPERTY()
	UMyGameInstance* GameInstance;

	// Guards against HandleSaveLoaded being called twice when streaming sublevels
	// trigger a second HandleWorldInitializedActors on the same world load.
	bool bDidRestoreMidCombat = false;
};