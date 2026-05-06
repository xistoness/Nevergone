// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/ItemTypes.h"
#include "InventoryContextMenuWidget.generated.h"

class UInventoryWidget;

// ---------------------------------------------------------------------------
// UInventoryContextMenuWidget
//
// The popup menu shown when the player clicks an inventory item slot.
// Buttons shown: Equip, Use, Drop — each conditionally visible.
//
// OwnerInventoryWidget must be set by UInventoryWidget immediately after
// creating this widget (via CreateWidget<>). GetTypedOuter is NOT used here
// because in UMG the widget outer is UWidgetTree, not the parent UserWidget.
//
// BP subclass responsibilities:
//   - Bind UButton::OnClicked to each On* method below.
//   - Implement BP_OnConfigured to show/hide buttons and update item labels.
//   - For consumables, show a slot-picker sub-panel and call OnEquipConsumableToSlot().
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UInventoryContextMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // Must be called by UInventoryWidget right after CreateWidget<>.
    // All button callbacks route through this reference.
    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void SetOwnerInventoryWidget(UInventoryWidget* Owner) { OwnerInventoryWidget = Owner;};

    // Configures the menu for the given inventory entry.
    // bCanEquip/Use/Drop control which buttons BP should show.
    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void Configure(const FInventoryEntry& Entry, bool bCanEquip, bool bCanUse, bool bCanDrop);

    // -----------------------------------------------------------------------
    // Button callbacks — bind in BP subclass via UButton::OnClicked
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void OnEquipPressed();

    // Called when the player chooses which consumable slot to assign the item to.
    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void OnEquipConsumableToSlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void OnUsePressed();

    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void OnDropPressed();

    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void OnCancelPressed();

    // -----------------------------------------------------------------------
    // Read access for BP — drives button visibility
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintPure, Category = "Inventory|ContextMenu")
    bool GetCanEquip()     const { return bCanEquip; }

    UFUNCTION(BlueprintPure, Category = "Inventory|ContextMenu")
    bool GetCanUse()       const { return bCanUse; }

    UFUNCTION(BlueprintPure, Category = "Inventory|ContextMenu")
    bool GetCanDrop()      const { return bCanDrop; }

    // True when the pending item is a consumable — BP should show the slot-picker.
    UFUNCTION(BlueprintPure, Category = "Inventory|ContextMenu")
    bool GetIsConsumable() const { return bIsConsumable; }

protected:

    // Called after Configure() — BP updates button visibility and item info labels.
    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|ContextMenu")
    void BP_OnConfigured(const FInventoryEntry& Entry, bool bEquip, bool bUse, bool bDrop);

private:

    // Direct reference to the owning InventoryWidget.
    // Set via SetOwnerInventoryWidget(); never resolved via outer chain.
    UPROPERTY()
    UInventoryWidget* OwnerInventoryWidget = nullptr;

    FInventoryEntry CurrentEntry;
    bool bCanEquip     = false;
    bool bCanUse       = false;
    bool bCanDrop      = false;
    bool bIsConsumable = false;
};
