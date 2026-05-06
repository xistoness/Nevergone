// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/ItemTypes.h"
#include "EquipmentSlotWidget.generated.h"

class UInventoryWidget;
class UItemDefinition;

// ---------------------------------------------------------------------------
// UEquipmentSlotWidget
//
// Represents one equipment or consumable slot in the right panel of the
// inventory screen (shown per-character alongside the 3D render).
//
// Used for both gear slots (Armor/Weapon/Necklace/Belt) and consumable hotbar
// slots [0..3]. The SlotType and ConsumableIndex fields differentiate them.
//
// BP subclass responsibilities:
//   - Show slot-type icon (armor silhouette, sword icon, etc.) when empty.
//   - Show equipped item icon and name when occupied.
//   - Call appropriate InventoryWidget callback on click:
//       Gear slots:       GetOwningInventoryWidget()->OnEquipmentSlotClicked(SlotType)
//       Consumable slots: GetOwningInventoryWidget()->OnConsumableSlotClicked(ConsumableIndex)
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UEquipmentSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // Configures this widget as a gear slot (Armor, Weapon, Necklace, Belt).
    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    void InitAsEquipmentSlot(EEquipmentSlot InSlot);

    // Configures this widget as a consumable hotbar slot [0..3].
    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    void InitAsConsumableSlot(int32 InConsumableIndex);

    // Refreshes the visual with the currently equipped item (pass null to show empty).
    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
    void SetEquippedItem(UItemDefinition* ItemDef);

    UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
    EEquipmentSlot GetSlotType() const { return SlotType; }

    UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
    int32 GetConsumableIndex() const { return ConsumableIndex; }

    UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
    bool IsConsumableSlot() const { return bIsConsumableSlot; }

    UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
    bool IsOccupied() const { return CurrentItem != nullptr; }
    
    // Direct reference to the owning InventoryWidget.
    // Set via SetOwnerInventoryWidget(); never resolved via outer chain.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Inventory")
    UInventoryWidget* OwnerInventoryWidget = nullptr;
    
    // Must be called by UInventoryWidget right after CreateWidget<>.
    // All button callbacks route through this reference.
    UFUNCTION(BlueprintCallable, Category = "Inventory|ContextMenu")
    void SetOwnerInventoryWidget(UInventoryWidget* Owner) { OwnerInventoryWidget = Owner;};

protected:

    // BP shows the item icon and name; called when an item is equipped.
    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Equipment")
    void BP_OnItemEquipped(UItemDefinition* ItemDef, UTexture2D* Icon);

    // BP shows the empty slot placeholder; called when the slot is cleared.
    UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Equipment")
    void BP_OnSlotCleared(EEquipmentSlot EquipSlot, bool bIsConsumable);

private:

    EEquipmentSlot SlotType         = EEquipmentSlot::None;
    int32          ConsumableIndex  = INDEX_NONE;
    bool           bIsConsumableSlot = false;

    UPROPERTY()
    UItemDefinition* CurrentItem = nullptr;
};
