// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/ItemTypes.h"
#include "Data/PartyData.h"
#include "Interfaces/ItemSlotOwner.h"
#include "World/ItemUsageSubsystem.h"
#include "InventoryWidget.generated.h"

class UInventorySubsystem;
class UItemUsageSubsystem;
class UItemDefinition;
class ACharacterBase;
class UTextureRenderTarget2D;
class UInventoryContextMenuWidget;
class UWidget;
class UCharacterPreviewSubsystem;

// ---------------------------------------------------------------------------
// EInventoryPanel
//
// Identifies which of the two focusable panels the cursor is currently in.
// Left  = item grid (6 cols x 5 rows = 30 slots).
// Right = equipment panel (1 col, 8 rows: 4 gear + 4 consumables).
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EInventoryPanel : uint8
{
    Left   UMETA(DisplayName = "Item Grid"),
    Right  UMETA(DisplayName = "Equipment"),
};

// ---------------------------------------------------------------------------
// UInventoryWidget
//
// Root widget for the full inventory screen.
//
// Navigation model (mirrors PartyManagerWidget):
//   Left panel:  6-column x 5-row grid. Up/Down/Left/Right move the cursor.
//                NavigateRight from column 5 → jumps to right panel.
//   Right panel: single column of 8 slots (rows 0-3 = gear, 4-7 = consumables).
//                NavigateLeft → returns to left panel.
//   Confirm (UIConfirm) → opens context menu on the focused slot.
//   Cancel          → closes context menu if open, otherwise signals controller to close.
//
// Context menu positioning:
//   CapturePositionFromWidget(UWidget*) stores the anchor from the widget's
//   geometry — safe for both mouse clicks and joystick navigation.
//   BP calls GetContextMenuPosition() to pass to SetPositionInViewport.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UInventoryWidget : public UUserWidget, public IItemSlotOwner
{
    GENERATED_BODY()

public:

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // -----------------------------------------------------------------------
    // Open / Close
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void OpenInventory();

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void CloseInventory();

    // -----------------------------------------------------------------------
    // Input navigation (called by ExplorationPlayerController / BattlePreparationController)
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
    void NavigateUp();

    UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
    void NavigateDown();

    UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
    void NavigateLeft();

    UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
    void NavigateRight();

    // Confirms the currently focused slot — opens its context menu.
    // Returns false when the context menu is already open (input absorbed by menu).
    UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
    bool ConfirmSelection();

    // Closes the context menu if open (returns true, widget stays open).
    // Returns false when nothing was open — controller should close the inventory.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Navigation")
    bool CancelSelection();

    // -----------------------------------------------------------------------
    // Character selection (right panel)
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Inventory|Character")
    void SelectNextCharacter();

    UFUNCTION(BlueprintCallable, Category = "Inventory|Character")
    void SelectPreviousCharacter();

    UFUNCTION(BlueprintCallable, Category = "Inventory|Character")
    void SelectCharacterByIndex(int32 Index);

    UFUNCTION(BlueprintPure, Category = "Inventory|Character")
    int32 GetSelectedCharacterIndex() const { return SelectedCharacterIndex; }

    // -----------------------------------------------------------------------
    // Item interaction — called by slot widgets (mouse) or ConfirmSelection (input)
    // -----------------------------------------------------------------------

    // OriginWidget is the button/slot widget that triggered the action.
    // Its geometry is used to anchor the context menu position.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Interaction")
    void OnInventorySlotClicked(int32 SlotIndex, UWidget* OriginWidget);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Interaction")
    void OnEquipmentSlotClicked(EEquipmentSlot EquipSlot, UWidget* OriginWidget);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Interaction")
    void OnConsumableSlotClicked(int32 SlotIndex, UWidget* OriginWidget);

    // Returns the viewport position to use for SetPositionInViewport on the context menu.
    // Derived from the last focused/clicked slot widget's geometry — works for
    // both mouse and joystick navigation.
    UFUNCTION(BlueprintPure, Category = "Inventory|ContextMenu")
    FVector2D GetContextMenuPosition() const { return LastContextMenuPosition; }

    // -----------------------------------------------------------------------
    // Context menu callbacks (called by UInventoryContextMenuWidget)
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void OnContextMenuEquip();

    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void OnContextMenuEquipConsumable(int32 TargetSlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void OnContextMenuUse();

    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void OnContextMenuDrop();

    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void OnContextMenuCancel();

    // -----------------------------------------------------------------------
    // Sort
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Inventory|Sort")
    void SetSortLayers(const TArray<FInventorySortLayer>& NewLayers);

    // -----------------------------------------------------------------------
    // Weight helpers
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintPure, Category = "Inventory|Weight")
    float GetCurrentWeight() const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Weight")
    float GetMaxWeight() const;

protected:

    // -----------------------------------------------------------------------
    // BP implementable events
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
    void BP_RefreshInventorySlots(const TArray<FInventoryEntry>& Entries);

    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
    void BP_RefreshEquipmentPanel(int32 CharacterIndex, const FEquipmentLoadout& Loadout);

    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void RegisterContextMenuWidget(UInventoryContextMenuWidget* Widget);

    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
    void BP_ShowContextMenu(const FInventoryEntry& Entry, bool bCanEquip, bool bCanUse, bool bCanDrop);

    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
    void BP_HideContextMenu();

    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
    void BP_SetCharacterInfo(const FText& CharacterName, const FText& ClassName);

    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
    void BP_SetCharacterRenderTarget(UTextureRenderTarget2D* RenderTarget);

    // Pushes the selected character's persistent stats to the right panel so
    // the player can see HP at a glance while deciding which consumable to use.
    // Called from RefreshAll() whenever the inventory or roster changes.
    // Status effects are not included here yet — see comment in cpp for the
    // status-persistence design note.
    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
    void BP_SetCharacterStats(int32 CurrentHP, int32 MaxHP, bool bIsAlive);

    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
    void BP_RefreshWeightDisplay(float Current, float Max, bool bIsOverweight);

    // Pushes the current party gold to the UI. Called from RefreshAll() and
    // from the OnGoldChanged subsystem delegate, so BP only needs to bind
    // this one event (no manual polling).
    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
    void BP_RefreshGoldDisplay(int32 CurrentGold);

    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
    void BP_RefreshCharacterNavigation(int32 CurrentIndex, int32 TotalMembers);

    // Fired after an item use attempt resolves (success or failure).
    // BP can use this to show feedback (toast, sound, screen shake).
    // Item is the consumable that was used / blocked. Result == EItemUseResult::Success
    // means the item was consumed; any other value means no state changed.
    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
    void BP_OnItemUseResult(UItemDefinition* Item, EItemUseResult Result);

    // Called when the cursor moves to a new slot so BP can update the highlight visual.
    // FlatIndex is the resolved slot index: 0-29 for left panel, 30-37 for right panel.
    // INDEX_NONE means the cursor is in an empty area.
    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Navigation")
    void BP_UpdateCursorVisual(EInventoryPanel Panel, int32 Col, int32 Row, int32 FlatIndex);

private:

    // -----------------------------------------------------------------------
    // Subsystem
    // -----------------------------------------------------------------------

    UPROPERTY()
    UInventorySubsystem* InventorySubsystem = nullptr;

    // -----------------------------------------------------------------------
    // Character selection
    // -----------------------------------------------------------------------

    int32 SelectedCharacterIndex = 0;

    // -----------------------------------------------------------------------
    // Context menu pending state
    // -----------------------------------------------------------------------

    int32 PendingSlotIndex = INDEX_NONE;
    EEquipmentSlot PendingEquipmentSlot = EEquipmentSlot::None;
    bool bPendingIsConsumable = false;
    int32 PendingConsumableSlot = INDEX_NONE;

    // True while a context menu is open — navigation input is absorbed until closed.
    bool bContextMenuOpen = false;

    // Reference to the currently open context menu widget.
    // Set in RegisterContextMenuWidget, cleared when the menu is hidden or the inventory closes.
    UPROPERTY()
    UInventoryContextMenuWidget* ActiveContextMenuWidget = nullptr;

    // -----------------------------------------------------------------------
    // Context menu position
    // -----------------------------------------------------------------------

    // Viewport position derived from the last focused slot widget's geometry.
    // Updated by CapturePositionFromWidget() before every BP_ShowContextMenu call.
    FVector2D LastContextMenuPosition = FVector2D::ZeroVector;

    // -----------------------------------------------------------------------
    // 2D cursor state
    // -----------------------------------------------------------------------

    // Which panel currently has focus.
    EInventoryPanel ActivePanel = EInventoryPanel::Left;

    // Left panel cursor: 6 columns (0-5), 5 rows (0-4).
    static constexpr int32 GridCols = 6;
    static constexpr int32 GridRows = 5;
    int32 CursorCol = 0;
    int32 CursorRow = 0;

    // Right panel cursor: 1 column, 8 rows (0-3 gear, 4-7 consumables).
    static constexpr int32 EquipRows = 8;
    int32 EquipCursorRow = 0;

    // -----------------------------------------------------------------------
    // SceneCapture
    // -----------------------------------------------------------------------

    // Delegates all preview rendering to the shared CharacterPreviewSubsystem.
    // No SceneCapture2D, DirectionalLight, or pawn is owned here.
    UPROPERTY()
    class UCharacterPreviewSubsystem* PreviewSubsystem = nullptr;

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    void BindSubsystemDelegates();
    void UnbindSubsystemDelegates();
    void HandleInventoryChanged();
    void HandleWeightChanged(float NewWeight);
    void HandleGoldChanged(int32 NewGold);
    void RefreshAll();
    void SetupSceneCapture();
    void TeardownSceneCapture();
    FGuid GetSelectedCharacterGUID() const;
    TArray<FPartyMemberData*> GetActivePartyMembers() const;
    void ClampCharacterIndex();
    bool Entries_IsValidPendingSlot() const;

    // Computes LastContextMenuPosition from a widget's absolute geometry.
    // Anchors the menu below the widget with a small gap.
    void CapturePositionFromWidget(UWidget* OriginWidget);

    // Broadcasts BP_UpdateCursorVisual with the current cursor state.
    void BroadcastCursorVisual();

    // Returns the flat inventory slot index (0-29) for the current left panel cursor.
    // Returns INDEX_NONE if the slot is empty or out of range.
    int32 GetFocusedInventorySlotIndex() const;

    // Returns the equipment slot enum for the current right panel cursor (rows 0-3).
    EEquipmentSlot GetFocusedEquipmentSlot() const;

    // Opens the context menu for whichever slot is currently focused by the cursor.
    // Used by ConfirmSelection() to unify mouse and input paths.
    void OpenContextMenuForFocusedSlot();
};
