// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/CharacterBase.h"
#include "Interfaces/Interactable.h"
#include "Types/DialogueSystemTypes.h"
#include "DialogueNPC.generated.h"

class UDialogueData;
class USaveableComponent;

// ---------------------------------------------------------------------------
// FDialogueEntryPoint
//
// One candidate conversation for an NPC. The subsystem evaluates the array
// from index 0 downward and uses the first entry whose Condition is met.
// The last entry should always have Condition.Type = None as a guaranteed
// fallback.
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FDialogueEntryPoint
{
    GENERATED_BODY()

    // The conversation asset to use when this entry point is selected.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dialogue")
    TObjectPtr<UDialogueData> DialogueAsset = nullptr;

    // Which node index in DialogueAsset to use as the entry point.
    // 0 = default start. Override this to begin mid-conversation after
    // a game state change (e.g. post-quest dialogue starts at node 5).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dialogue")
    int32 StartNodeIndex = 0;

    // Condition that must be satisfied for this entry point to be chosen.
    // Set Type = None for an unconditional fallback.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dialogue")
    FDialogueCondition Condition;
};

// ---------------------------------------------------------------------------
// ADialogueNPC
// ---------------------------------------------------------------------------

/**
 * ADialogueNPC
 *
 * A world NPC capable of initiating dialogue when the player interacts with them.
 *
 * Conversation selection works as a priority list: DialogueEntryPoints is evaluated
 * top-to-bottom and the first entry whose Condition is met is used. This allows
 * the same NPC to branch into completely different conversations depending on quest
 * state, flags, or affinity — without duplicating node content across assets.
 *
 * Affinity starts at InitialAffinity and is tracked externally by NPCAffinitySubsystem
 * (keyed by the SaveableComponent GUID). The subsystem only writes the initial value
 * if no save data already exists for this NPC, so saves are never overwritten.
 *
 * Usage:
 *  1. Create a Blueprint subclass of ADialogueNPC.
 *  2. Populate DialogueEntryPoints (last entry should be unconditional fallback).
 *  3. Set InitialAffinity if this NPC should not start at neutral (0).
 *  4. Place in the level.
 */
UCLASS()
class NEVERGONE_API ADialogueNPC : public ACharacterBase, public IInteractable
{
    GENERATED_BODY()

public:

    ADialogueNPC();

    // --- IInteractable ---
    virtual void Interact_Implementation(AActor* Interactor) override;

    // --- Identity ---

    // Returns the GUID from this NPC's SaveableComponent.
    // Used as the key in UNPCAffinitySubsystem.
    FGuid GetNpcGuid() const;

protected:

    virtual void BeginPlay() override;

    // Ordered list of candidate conversations. Evaluated top-to-bottom;
    // the first entry with a satisfied Condition is used.
    // The final entry should always be an unconditional fallback (Condition.Type = None).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
    TArray<FDialogueEntryPoint> DialogueEntryPoints;

    // Starting affinity value for this NPC on a fresh save.
    // Positive = friendly, negative = hostile.
    // Ignored if the NPC already has an entry in the save's NPCAffinityMap.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dialogue|Affinity")
    int32 InitialAffinity = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
    UInteractableComponent* InteractableComponent;
};
