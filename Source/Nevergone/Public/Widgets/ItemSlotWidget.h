// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/ItemTypes.h"
#include "Interfaces/ItemSlotOwner.h"
#include "ItemSlotWidget.generated.h"

class UItemDefinition;

// ---------------------------------------------------------------------------
// UItemSlotWidget
//
// Represents one of the 30 inventory slots in the left panel.
// Instantiated by UInventoryWidget::BP_RefreshInventorySlots for each slot,
// and reused by UContainerTransferWidget for both the player and container
// panels (via the IItemSlotOwner interface).
//
// BP subclass responsibilities:
//   - Show/hide empty-slot placeholder vs occupied-slot visual.
//   - Display item icon, quantity badge, and weight badge.
//   - Highlight the slot on hover.
//   - On click, dispatch via IItemSlotOwner::Execute_HandleSlotClicked
//     (Owner=GetOwnerObject, SlotIndex=GetSlotIndex, OriginWidget=self).
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UItemSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // Called by the parent widget to populate this slot with an item.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Slot")
    void SetSlotData(int32 InSlotIndex, const FInventoryEntry& InEntry);

    // Called by the parent widget to mark this slot as empty.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Slot")
    void ClearSlot(int32 InSlotIndex);

    UFUNCTION(BlueprintPure, Category = "Inventory|Slot")
    int32 GetSlotIndex() const { return SlotIndex; }

    UFUNCTION(BlueprintPure, Category = "Inventory|Slot")
    bool IsEmpty() const { return !CurrentEntry.IsValid(); }

    UFUNCTION(BlueprintPure, Category = "Inventory|Slot")
    const FInventoryEntry& GetEntry() const { return CurrentEntry; }

    // Direct reference to the owning widget. Implements IItemSlotOwner so the
    // BP click handler can dispatch back without knowing the concrete owner
    // class. Set via SetSlotOwner(); never resolved via outer chain.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Inventory",
              meta = (MustImplement = "/Script/Nevergone.ItemSlotOwner"))
    TScriptInterface<IItemSlotOwner> SlotOwner;

    // Must be called by the parent widget right after CreateWidget<>.
    // All button callbacks route through this reference.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Slot")
    void SetSlotOwner(const TScriptInterface<IItemSlotOwner>& Owner) { SlotOwner = Owner; }

    // Convenience accessor: returns the underlying UObject of SlotOwner so BP
    // can plug it directly into IItemSlotOwner::HandleSlotClicked's "Target"
    // pin (Execute_HandleSlotClicked node). Null when no owner is set.
    UFUNCTION(BlueprintPure, Category = "Inventory|Slot")
    UObject* GetSlotOwnerObject() const { return SlotOwner.GetObject(); }

protected:

    // Called when slot data changes — BP updates icon, quantity, and weight badge.
    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Slot")
    void BP_OnSlotPopulated(const FInventoryEntry& Entry, UTexture2D* Icon, int32 Quantity, float Weight);

    // Called when the slot is cleared — BP shows the empty placeholder.
    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Slot")
    void BP_OnSlotCleared();

private:

    int32 SlotIndex = INDEX_NONE;
    FInventoryEntry CurrentEntry;
};
