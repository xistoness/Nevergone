// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/QuestData.h"
#include "QuestLogWidget.generated.h"

class UQuestSubsystem;

/**
 * UQuestLogWidget
 *
 * Quest diary UI. Displays Active, Completed, and Failed quests.
 *
 * Navigation model (mirrors DialogueWidget and PartyManagerWidget):
 *  - NavigateNext / NavigatePrevious move the cursor through quest title entries.
 *  - ConfirmSelection expands the highlighted entry (shows description + steps).
 *  - CancelSelection collapses a selected entry, or signals close if none selected.
 *
 * BP implementation contract:
 *  - BP_ClearQuestLog: remove all quest entry widgets.
 *  - BP_AddQuestEntry: append a quest title entry and return the index assigned to it.
 *  - BP_AddQuestStep: add a step to the last opened entry.
 *  - BP_SetSelectedEntry: apply/remove highlight for the given entry index.
 *  - BP_SetEntryExpanded: show or hide description + steps for the given entry index.
 *  - BP_BeginQuestEntry / BP_EndQuestEntry: bracket each quest block (unchanged).
 */
UCLASS()
class NEVERGONE_API UQuestLogWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION(BlueprintCallable, Category = "Quest Log")
    void RefreshQuestLog();

    // --- Keyboard navigation (called by ExplorationPlayerController) ---

    UFUNCTION(BlueprintCallable, Category = "Quest Log")
    void NavigateDown();

    UFUNCTION(BlueprintCallable, Category = "Quest Log")
    void NavigateUp();

    // Expands the highlighted quest entry (same as clicking it).
    UFUNCTION(BlueprintCallable, Category = "Quest Log")
    void ConfirmSelection();

    // Total number of entries currently listed (set during RefreshQuestLog).
    UFUNCTION(BlueprintPure, Category = "Quest Log")
    int32 GetEntryCount() const { return EntryCount; }

protected:

    UFUNCTION(BlueprintImplementableEvent, Category = "Quest Log")
    void BP_ClearQuestLog();

    UFUNCTION(BlueprintImplementableEvent, Category = "Quest Log")
    void BP_BeginQuestEntry(const FText& Title, const FText& Description, EQuestState State);

    UFUNCTION(BlueprintImplementableEvent, Category = "Quest Log")
    void BP_AddQuestStep(const FText& DiaryEntry, bool bCompleted);

    UFUNCTION(BlueprintImplementableEvent, Category = "Quest Log")
    void BP_EndQuestEntry();

    UFUNCTION(BlueprintImplementableEvent, Category = "Quest Log")
    void BP_Confirm();
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Quest Log")
    void BP_NavigateUp();
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Quest Log")
    void BP_NavigateDown();
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Quest Log")
    void BP_StartQuestLog();

private:

    void HandleQuestStateChanged(FName QuestName, EQuestState NewState);
    void HandleQuestStepAdvanced(FName QuestName, int32 StepIndex);
    UQuestData* LoadQuestAsset(FName QuestName) const;

    UPROPERTY()
    UQuestSubsystem* QuestSubsystem = nullptr;

    // Total number of quest entries in the current list (set during RefreshQuestLog).
    int32 EntryCount = 0;
};