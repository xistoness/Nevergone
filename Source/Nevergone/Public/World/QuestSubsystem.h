// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/QuestData.h"
#include "QuestSubsystem.generated.h"

// Fired when a quest's EQuestState changes (started, completed, failed).
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnQuestStateChanged, FName /*QuestName*/, EQuestState /*NewState*/);

// Fired when a quest step is completed — used to refresh diary UI without full rebuild.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnQuestStepAdvanced, FName /*QuestName*/, int32 /*StepIndex*/);

/**
 * UQuestSubsystem
 *
 * Tracks runtime quest state keyed by quest FName (primary asset name).
 * Quests are modelled as directed graphs of FQuestStep nodes; completing a step
 * unlocks subsequent steps as defined in UQuestData.
 *
 * Persisted via MySaveGame::QuestRuntimeStates.
 */
UCLASS()
class NEVERGONE_API UQuestSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // --- Quest lifecycle ---

    // Starts the quest if it was NotStarted. Unlocks initial steps from the asset.
    UFUNCTION(BlueprintCallable, Category = "Quests")
    void StartQuest(FName QuestName);

    // Marks the quest as Completed regardless of step state. Use for scripted endings.
    UFUNCTION(BlueprintCallable, Category = "Quests")
    void CompleteQuest(FName QuestName);

    // Marks the quest as Failed regardless of step state.
    UFUNCTION(BlueprintCallable, Category = "Quests")
    void FailQuest(FName QuestName);

    // --- Step advancement ---

    // Completes the given step of an active quest.
    // - Marks the step as completed and removes it from UnlockedStepIndices.
    // - Unlocks the steps listed in FQuestStep::UnlocksStepIndices.
    // - If bFailsQuest is set, calls FailQuest. If bFinishesQuest, calls CompleteQuest.
    // - No-ops if the step is not in UnlockedStepIndices or the quest is not Active.
    UFUNCTION(BlueprintCallable, Category = "Quests")
    void AdvanceQuestStep(FName QuestName, int32 StepIndex);

    // --- Queries ---

    UFUNCTION(BlueprintCallable, Category = "Quests")
    EQuestState GetQuestState(FName QuestName) const;

    UFUNCTION(BlueprintCallable, Category = "Quests")
    bool IsQuestActive(FName QuestName) const;

    UFUNCTION(BlueprintCallable, Category = "Quests")
    bool IsQuestCompleted(FName QuestName) const;

    UFUNCTION(BlueprintCallable, Category = "Quests")
    bool IsStepCompleted(FName QuestName, int32 StepIndex) const;

    UFUNCTION(BlueprintCallable, Category = "Quests")
    bool IsStepUnlocked(FName QuestName, int32 StepIndex) const;

    // Returns the full runtime state for a quest (for UI / save queries).
    bool GetRuntimeState(FName QuestName, FQuestRuntimeState& OutState) const;

    // Returns all quests currently in the given state (used by quest log UI).
    UFUNCTION(BlueprintCallable, Category = "Quests")
    TArray<FName> GetQuestsInState(EQuestState State) const;

    // --- Delegates ---

    FOnQuestStateChanged  OnQuestStateChanged;
    FOnQuestStepAdvanced  OnQuestStepAdvanced;

    // --- Persistence ---

    void SyncFromGameInstance();
    void FlushToGameInstance();
    
    // Clears all quest runtime states from memory and flushes the empty state to GameInstance.
    void Reset();

private:

    // Loads the UQuestData asset for the given quest name synchronously.
    // Returns nullptr if the asset is not found at the conventional path.
    UQuestData* LoadQuestAsset(FName QuestName) const;

    // QuestName -> runtime state
    TMap<FName, FQuestRuntimeState> QuestRuntimeStates;
};
