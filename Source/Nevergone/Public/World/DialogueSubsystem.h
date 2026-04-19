// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/DialogueSystemTypes.h"
#include "DialogueSubsystem.generated.h"

class UDialogueData;
class ADialogueNPC;
class ACharacterBase;

DECLARE_MULTICAST_DELEGATE(FOnDialogueStarted);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDialogueNodeReady, const FDialogueNode& /*Node*/);
DECLARE_MULTICAST_DELEGATE(FOnDialogueEnded);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnItemRequested, FName /*ItemId*/);

/**
 * UDialogueSubsystem
 *
 * Central coordinator for all dialogue interactions.
 * Conversation entry is driven by ADialogueNPC, which selects the appropriate
 * UDialogueData asset and StartNodeIndex based on a priority list of conditions.
 */
UCLASS()
class NEVERGONE_API UDialogueSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    // Starts a dialogue session using the given asset, entering at StartNodeIndex.
    // Called by ADialogueNPC::Interact after selecting the correct entry point.
    bool StartDialogue(UDialogueData* DialogueData, ADialogueNPC* NPC, int32 StartNodeIndex = 0);

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void SelectChoice(int32 ChoiceIndex);

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void EndDialogue();

    // Evaluates a condition against the current world/player state.
    // Public so ADialogueNPC can evaluate entry point conditions before calling StartDialogue.
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    bool EvaluateCondition(const FDialogueCondition& Condition) const;

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    bool IsDialogueActive() const { return bSessionActive; }

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    bool GetCurrentNode(FDialogueNode& OutNode) const;

    FOnDialogueStarted   OnDialogueStarted;
    FOnDialogueNodeReady OnDialogueNodeReady;
    FOnDialogueEnded     OnDialogueEnded;
    FOnItemRequested     OnItemRequested;

private:

    void ApplyEffects(const TArray<FDialogueEffect>& Effects);
    void DisplayNode(int32 NodeIndex);
    class UUnitStatsComponent* GetPlayerStatsComponent() const;

    bool bSessionActive = false;

    UPROPERTY()
    UDialogueData* ActiveDialogueData = nullptr;

    UPROPERTY()
    ADialogueNPC* ActiveNPC = nullptr;

    int32 CurrentNodeIndex = INDEX_NONE;
};
