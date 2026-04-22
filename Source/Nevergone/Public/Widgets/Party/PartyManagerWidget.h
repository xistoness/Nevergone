// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/PartyData.h"
#include "PartyManagerWidget.generated.h"

class UPartyMemberEntryWidget;
class UPartyManagerSubsystem;

/**
 * UPartyManagerWidget
 *
 * Party management screen accessible during exploration.
 * Two-column layout: Active Party (left, max 4) and Roster/Bench (right).
 *
 * Navigation model — 2D grid cursor:
 *  - CursorCol 0 = Active panel, CursorCol 1 = Bench panel.
 *  - NavigateUp / NavigateDown move within a column.
 *  - NavigateLeft / NavigateRight switch columns, clamping row to the
 *    destination column's last entry if needed.
 *
 * Two-step swap flow:
 *  1. Confirm on a bench member  → PendingSwapIncomingID is set.
 *  2. Confirm on an active member → SwapActiveMembers is called and roster rebuilds.
 *  Cancel at step 2 clears PendingSwapIncomingID without closing.
 *  Cancel with no pending swap → returns false (controller closes the widget).
 *
 * BP implementation contract:
 *  - BP_ClearEntries: remove all dynamically created entry widgets.
 *  - BP_AddActiveEntry / BP_AddBenchEntry: append a card to the correct panel.
 *  - BP_SetSwapPendingState: show/hide the "select who to replace" prompt.
 */
UCLASS()
class NEVERGONE_API UPartyManagerWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // Forces a full rebuild of both panels from subsystem data.
    UFUNCTION(BlueprintCallable, Category = "Party Manager")
    void RefreshPartyManager();

    // --- 2D keyboard navigation (called by ExplorationPlayerController) ---

    UFUNCTION(BlueprintCallable, Category = "Party Manager")
    void NavigateUp();

    UFUNCTION(BlueprintCallable, Category = "Party Manager")
    void NavigateDown();

    UFUNCTION(BlueprintCallable, Category = "Party Manager")
    void NavigateLeft();

    UFUNCTION(BlueprintCallable, Category = "Party Manager")
    void NavigateRight();

    // Confirms the currently highlighted entry.
    // On a bench member: marks it as swap candidate (step 1).
    // On an active member with pending swap: executes the swap (step 2).
    UFUNCTION(BlueprintCallable, Category = "Party Manager")
    void ConfirmSelection();

    // Cancels a pending swap (returns true, widget stays open) or signals
    // the controller to close (returns false, nothing was pending).
    UFUNCTION(BlueprintCallable, Category = "Party Manager")
    bool CancelSelection();

protected:

    UFUNCTION(BlueprintImplementableEvent, Category = "Party Manager")
    void BP_ClearEntries();

    UFUNCTION(BlueprintImplementableEvent, Category = "Party Manager")
    void BP_AddActiveEntry(UPartyMemberEntryWidget* EntryWidget);

    UFUNCTION(BlueprintImplementableEvent, Category = "Party Manager")
    void BP_AddBenchEntry(UPartyMemberEntryWidget* EntryWidget);

    // Called when a swap source is selected or cleared.
    UFUNCTION(BlueprintImplementableEvent, Category = "Party Manager")
    void BP_SetSwapPendingState(bool bPending, const FText& IncomingName);

    UPROPERTY(EditDefaultsOnly, Category = "Party Manager|UI")
    TSubclassOf<UPartyMemberEntryWidget> MemberEntryWidgetClass;

private:

    void UpdateSelectionVisual();

    // Resolves the flat AllEntries index from a (col, row) pair.
    // Returns INDEX_NONE if the cell is empty (e.g. empty active slot).
    int32 GetEntryIndex(int32 InCol, int32 InRow) const;

    // Returns the number of entries in a given column.
    int32 GetColumnSize(int32 InCol) const;

    // Clamps CursorRow to the last valid row in CursorCol.
    void ClampCursor();

    // Flat list of all visible entries: active first, then bench.
    // Rebuilt on every RefreshPartyManager call.
    UPROPERTY()
    TArray<UPartyMemberEntryWidget*> AllEntries;

    // Number of active entries at the front of AllEntries.
    int32 ActiveEntryCount = 0;

    // 2D cursor: col 0 = active panel, col 1 = bench panel.
    int32 CursorCol = 0;
    int32 CursorRow = 0;
    
    // State machine for the two-step active-member confirm flow.
    // None       = no selection in progress
    // Selected   = first confirm on an active member; waiting for second input
    enum class EActiveConfirmState : uint8 { None, Selected };
    EActiveConfirmState ActiveConfirmState = EActiveConfirmState::None;

    // CharacterID of the active member highlighted in step 1.
    FGuid PendingActiveID;

    // CharacterID of the bench member chosen in step 1 of a swap.
    // Invalid GUID means no swap is pending.
    FGuid PendingSwapIncomingID;

    UPROPERTY()
    UPartyManagerSubsystem* PartySubsystem = nullptr;
};