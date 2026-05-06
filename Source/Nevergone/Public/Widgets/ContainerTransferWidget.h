// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/ItemSlotOwner.h"
#include "Types/ItemTypes.h"
#include "World/InventorySubsystem.h"  // EInventoryResult
#include "ContainerTransferWidget.generated.h"

class UInventorySubsystem;
class UItemContainerSubsystem;
class AContainerActor;
class UItemDefinition;

// ---------------------------------------------------------------------------
// UContainerTransferWidget
//
// Split-pane transfer UI: player's shared inventory on the left, the world
// container on the right. Confirm on a slot transfers one entire stack —
// when stack > 1 the BP_ShowQuantityPrompt is fired and the confirmation
// is deferred until ConfirmQuantitySelection is called back.
//
// The transfer pipeline itself goes through UItemContainerSubsystem so the
// widget never mutates inventory state directly. Equip / Use are NOT
// available from this screen by design.
//
// Lifecycle:
//   OpenForContainer  — bind delegates, refresh both panels, focus slot 0.
//   Close             — unbind, hide. Called by ExplorationPlayerController
//                       on UICancel.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UContainerTransferWidget : public UUserWidget, public IItemSlotOwner
{
    GENERATED_BODY()

public:

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // -----------------------------------------------------------------------
    // Open / Close
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Container")
    void OpenForContainer(AContainerActor* InContainer);

    UFUNCTION(BlueprintCallable, Category = "Container")
    void Close();

    UFUNCTION(BlueprintPure, Category = "Container")
    bool IsOpen() const { return bIsOpen; }

    // -----------------------------------------------------------------------
    // Input navigation (called by AExplorationPlayerController)
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Container|Navigation")
    void NavigateUp();

    UFUNCTION(BlueprintCallable, Category = "Container|Navigation")
    void NavigateDown();

    UFUNCTION(BlueprintCallable, Category = "Container|Navigation")
    void NavigateLeft();

    UFUNCTION(BlueprintCallable, Category = "Container|Navigation")
    void NavigateRight();

    UFUNCTION(BlueprintCallable, Category = "Container|Navigation")
    void FocusOnClicked(EContainerPanel FocusedPanel, int32 FocusedSlotIndex);
    
    // Confirms the focused slot. Stack > 1 → opens the quantity prompt.
    // Stack == 1 → transfers one unit immediately.
    // Returns false when the prompt is already open (input absorbed).
    UFUNCTION(BlueprintCallable, Category = "Container|Navigation")
    bool ConfirmSelection();

    // Closes the prompt if open (returns true; widget stays open).
    // Returns false when nothing was open — controller should close the widget.
    UFUNCTION(BlueprintCallable, Category = "Container|Navigation")
    bool CancelSelection();

    // -----------------------------------------------------------------------
    // Quantity prompt callback (BP-side slider widget calls this)
    // -----------------------------------------------------------------------

    // Called by BP after the player confirms a quantity in the prompt.
    // Quantity is clamped to [1, focused entry size].
    UFUNCTION(BlueprintCallable, Category = "Container|Prompt")
    void ConfirmQuantitySelection(int32 Quantity);

    // Called by BP if the player cancels the prompt without choosing.
    UFUNCTION(BlueprintCallable, Category = "Container|Prompt")
    void CancelQuantitySelection();

protected:

    // -----------------------------------------------------------------------
    // BlueprintImplementableEvents — visual layer
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintImplementableEvent, Category = "Container")
    void BP_OnOpened();

    UFUNCTION(BlueprintImplementableEvent, Category = "Container")
    void BP_OnClosed();

    UFUNCTION(BlueprintImplementableEvent, Category = "Container")
    void BP_RefreshPlayerSlots(const TArray<FInventoryEntry>& Entries);

    UFUNCTION(BlueprintImplementableEvent, Category = "Container")
    void BP_RefreshContainerSlots(const TArray<FInventoryEntry>& Entries);

    UFUNCTION(BlueprintImplementableEvent, Category = "Container")
    void BP_SetPlayerHeader(int32 UsedSlots, int32 MaxSlots, float CurrentWeight, float MaxWeight, bool bIsOverweight, int32 CurrentGold);

    UFUNCTION(BlueprintImplementableEvent, Category = "Container")
    void BP_SetContainerHeader(const FText& DisplayName, int32 UsedSlots, int32 MaxSlots, float TotalWeight);

    UFUNCTION(BlueprintImplementableEvent, Category = "Container")
    void BP_UpdateCursorVisual(EContainerPanel Panel, int32 FlatIndex);

    // ItemName: friendly label for the prompt header.
    // MinQuantity is always 1; MaxQuantity is the focused entry's stack size.
    UFUNCTION(BlueprintImplementableEvent, Category = "Container")
    void BP_ShowQuantityPrompt(const FText& ItemName, int32 MinQuantity, int32 MaxQuantity);

    UFUNCTION(BlueprintImplementableEvent, Category = "Container")
    void BP_HideQuantityPrompt();

    // Result == EInventoryResult::Success means the transfer succeeded.
    // Other codes (FailedInventoryFull, etc.) are surfaced for toast/sound feedback.
    UFUNCTION(BlueprintImplementableEvent, Category = "Container")
    void BP_OnTransferResult(EInventoryResult Result, EContainerPanel FromPanel);

private:

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    UPROPERTY(Transient)
    TObjectPtr<UInventorySubsystem> InventorySubsystem;

    UPROPERTY(Transient)
    TObjectPtr<UItemContainerSubsystem> TransferSubsystem;

    UPROPERTY(Transient)
    TWeakObjectPtr<AContainerActor> ActiveContainer;

    bool bIsOpen = false;

    // Currently focused panel and flat slot index within that panel.
    EContainerPanel FocusedPanel = EContainerPanel::PlayerInventory;
    int32           FocusedSlotIndex = 0;

    // Pending quantity prompt state — INDEX_NONE when no prompt is open.
    int32  PendingPromptSlotIndex = INDEX_NONE;
    EContainerPanel PendingPromptPanel = EContainerPanel::PlayerInventory;
    bool            bPromptOpen = false;

    // -----------------------------------------------------------------------
    // Subsystem delegate handlers
    // -----------------------------------------------------------------------

    void HandlePlayerInventoryChanged();
    void HandlePlayerGoldChanged(int32 NewGold);
    void HandleContainerContentsChanged();

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    // Refreshes both panels' slot lists + headers from the current state.
    void RefreshAll();

    // Returns the FInventoryEntry the cursor is currently on, or nullptr.
    const FInventoryEntry* GetFocusedEntry() const;

    // Returns the entry at SlotIndex on Panel, or nullptr.
    const FInventoryEntry* GetEntryAt(EContainerPanel Panel, int32 SlotIndex) const;

    // Returns how many slots a panel currently has populated.
    int32 GetUsedSlotsOnPanel(EContainerPanel Panel) const;

    // Clamps FocusedSlotIndex to a valid value for the focused panel.
    void ClampFocus();

    // Executes a transfer of Quantity units of the focused item to the other panel.
    // Skips validation if Quantity > available — caller should clamp first.
    void DoTransfer(int32 Quantity);
};
