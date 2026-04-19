// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NPCAffinitySubsystem.generated.h"

/**
 * UNPCAffinitySubsystem
 *
 * Tracks an integer affinity value per NPC, keyed by the NPC's SaveableComponent GUID.
 * Affinity can be used as a gate in FDialogueCondition (NPCAffinityMin).
 * Persisted via MySaveGame::NPCAffinityMap.
 *
 * Initialization flow:
 *  - ADialogueNPC::BeginPlay calls RegisterNPCIfNew(guid, InitialAffinity).
 *  - RegisterNPCIfNew only writes the value if the GUID is not already in the map,
 *    so loaded save data is never overwritten by the default value.
 */
UCLASS()
class NEVERGONE_API UNPCAffinitySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Registers the NPC with InitialValue only if it has no existing entry.
    // Safe to call every BeginPlay — no-ops if an entry already exists.
    UFUNCTION(BlueprintCallable, Category = "Affinity")
    void RegisterNPCIfNew(const FGuid& NpcGuid, int32 InitialValue);

    // Returns the current affinity for the given NPC GUID. 0 if unknown.
    UFUNCTION(BlueprintCallable, Category = "Affinity")
    int32 GetAffinity(const FGuid& NpcGuid) const;

    // Adds Delta to the current affinity (can be negative). Result is unclamped.
    UFUNCTION(BlueprintCallable, Category = "Affinity")
    void ChangeAffinity(const FGuid& NpcGuid, int32 Delta);

    // --- Persistence ---
    void SyncFromGameInstance();
    void FlushToGameInstance();

private:

    // NPC GUID -> affinity value
    TMap<FGuid, int32> AffinityMap;
};
