// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SaveableComponent.generated.h"


struct FActorSaveData;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class NEVERGONE_API USaveableComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	USaveableComponent();
	
	void OnRegister() override;
	
	// Returns the persistent GUID of this actor or creates one if there's none
	FGuid GetOrCreateGuid();

	// Assigns a persistent GUID (used on spawn / restore)
	void SetActorGuid(const FGuid& NewGuid);

	// Called before saving; actor fills SaveData
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

protected:

	/**
	 * Persistent GUID that identifies this actor across save/load cycles.
	 *
	 * EditAnywhere is intentional: the Unreal editor serializes this value
	 * into the level .uasset when the level is saved, so the GUID generated
	 * on first placement survives between play sessions and editor restarts.
	 *
	 * Do NOT modify this value manually. It is assigned automatically the
	 * first time the component registers (OnRegister → GetOrCreateGuid).
	 */
	UPROPERTY(EditAnywhere, Category = "Save")
	FGuid SaveGuid;
};