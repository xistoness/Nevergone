// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FlagsSubsystem.generated.h"

/**
 * UFlagsSubsystem
 *
 * Runtime gateway for global boolean flags (e.g. "defeated_boss_01", "found_secret_key").
 * Flags are persisted via MyGameInstance -> MySaveGame::GlobalFlags.
 * MySaveGame already owns the TMap<FName,bool>; this subsystem is the access layer.
 *
 * Sync pattern (same as PartyManagerSubsystem):
 *   - SyncFromGameInstance(): called after save load
 *   - FlushToGameInstance():  called before CommitSave()
 */
UCLASS()
class NEVERGONE_API UFlagsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	// --- Lifecycle ---
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// --- Flag access ---
	UFUNCTION(BlueprintCallable, Category = "Flags")
	bool HasFlag(FName Flag) const;

	UFUNCTION(BlueprintCallable, Category = "Flags")
	void SetFlag(FName Flag);

	UFUNCTION(BlueprintCallable, Category = "Flags")
	void ClearFlag(FName Flag);

	// --- Persistence ---

	// Pulls current flags from MyGameInstance into this subsystem's cache.
	// Call after loading a save slot.
	void SyncFromGameInstance();

	// Pushes cached flags back to MyGameInstance so CommitSave() picks them up.
	// Call before RequestSaveGame().
	void FlushToGameInstance();

private:

	// Runtime cache — mirrors MySaveGame::GlobalFlags
	TMap<FName, bool> Flags;
};