// Copyright Xyzto Works

#include "Widgets/InventoryContextMenuWidget.h"
#include "Widgets/InventoryWidget.h"
#include "Data/ItemDefinition.h"

void UInventoryContextMenuWidget::Configure(const FInventoryEntry& Entry, bool bInCanEquip, bool bInCanUse, bool bInCanDrop)
{
    CurrentEntry  = Entry;
    bCanEquip     = bInCanEquip;
    bCanUse       = bInCanUse;
    bCanDrop      = bInCanDrop;

    const UItemDefinition* Def = Entry.Definition.Get();
    bIsConsumable = Def && Def->Category == EItemCategory::Consumable;

    BP_OnConfigured(Entry, bCanEquip, bCanUse, bCanDrop);

    UE_LOG(LogTemp, Log, TEXT("[InventoryContextMenuWidget] Configured for '%s' — Equip=%d Use=%d Drop=%d Consumable=%d."),
        Def ? *Def->DisplayName.ToString() : TEXT("empty"),
        bCanEquip, bCanUse, bCanDrop, bIsConsumable);
}

void UInventoryContextMenuWidget::OnEquipPressed()
{
    if (!OwnerInventoryWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryContextMenuWidget] OnEquipPressed: OwnerInventoryWidget is null. Call SetOwnerInventoryWidget() after CreateWidget."));
        return;
    }

    // Gear items equip directly. Consumables wait for the BP slot-picker to call
    // OnEquipConsumableToSlot() with the chosen slot index.
    if (!bIsConsumable)
    {
        OwnerInventoryWidget->OnContextMenuEquip();
    }
}

void UInventoryContextMenuWidget::OnEquipConsumableToSlot(int32 SlotIndex)
{
    if (OwnerInventoryWidget)
    {
        OwnerInventoryWidget->OnContextMenuEquipConsumable(SlotIndex);
    }
}

void UInventoryContextMenuWidget::OnUsePressed()
{
    if (OwnerInventoryWidget)
    {
        OwnerInventoryWidget->OnContextMenuUse();
    }
}

void UInventoryContextMenuWidget::OnDropPressed()
{
    if (OwnerInventoryWidget)
    {
        OwnerInventoryWidget->OnContextMenuDrop();
    }
}

void UInventoryContextMenuWidget::OnCancelPressed()
{
    if (OwnerInventoryWidget)
    {
        OwnerInventoryWidget->OnContextMenuCancel();
    }
}
