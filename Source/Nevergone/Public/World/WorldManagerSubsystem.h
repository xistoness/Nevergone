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

private:

	// GameInstance listeners
	void HandleSaveLoaded();
	void HandlePartyRestored(const struct FPartyData& PartyData);

	// Core responsibilities
	void RestoreWorldState();
	void RestoreSaveableActors();
	void SpawnMissingActors();
	void ResolveActorConflicts();

	// Utilities
	void RegisterSaveableActors() const;
	void ClearCachedData();
	
	UPROPERTY()
	UMyGameInstance* GameInstance;
};
