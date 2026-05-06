// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SaveableComponent.generated.h"


struct FActorSaveData;

// ---------------------------------------------------------------------------
// USaveableComponent
//
// Marks an actor as participating in the persistent save system.
// Identifies the actor across save/load cycles via a stable FName "save id":
//
//   - For LEVEL-PLACED actors (bNetStartup == true):
//       The save id is the actor's FName, which is serialized in the level
//       .uasset by the engine and remains stable across all sessions.
//
//   - For RUNTIME-SPAWNED actors (bNetStartup == false):
//       The save id is generated at registration time as a stringified GUID.
//       Used for dropped items, combat units, and any other actor that does
//       not exist in the level asset.
//
// This identifier is what the world save system uses to match world actors
// against entries in the save game when a level is loaded.
// ---------------------------------------------------------------------------
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class NEVERGONE_API USaveableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USaveableComponent();

	virtual void OnRegister() override;

	// Returns the persistent identifier for this actor.
	// For placed actors, returns the actor's FName (stable, level-asset serialized).
	// For spawned actors, returns the generated runtime save id (FName stringified GUID).
	FName GetSaveIdentifier();

	// Manually sets the runtime save id (used after respawning a saved actor
	// so the new instance reclaims the same identifier as before).
	// No-op for placed actors — their id is the actor FName, set by the engine.
	void SetSaveIdentifier(FName NewIdentifier);

	// Called before saving; actor fills OutData
	void WriteSaveData(FActorSaveData& OutData) const;

	// Called after loading; actor restores internal state
	void ReadSaveData(const FActorSaveData& InData);

	TSoftClassPtr<AActor> GetActorClass() const;

	// Optional hook after full world restoration
	void OnPostRestore();

	// If false, this actor is saved within a session but never respawned on load.
	// Use for player pawns and other transient actors managed by the GameMode.
	UPROPERTY(EditAnywhere, Category = "Save")
	bool bPersistAcrossLoads = true;

private:

	// Runtime-only identifier for spawned actors. Empty for placed actors
	// (which are identified by GetOwner()->GetFName()).
	// Generated lazily on first GetSaveIdentifier() call when the owner is
	// not a NetStartup actor, or set explicitly via SetSaveIdentifier when
	// respawning a saved actor.
	UPROPERTY(Transient)
	FName RuntimeSaveId;
};
