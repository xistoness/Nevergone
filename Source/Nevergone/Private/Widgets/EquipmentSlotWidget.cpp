// Copyright Xyzto Works

#include "Widgets/EquipmentSlotWidget.h"
#include "Data/ItemDefinition.h"

void UEquipmentSlotWidget::InitAsEquipmentSlot(EEquipmentSlot InSlot)
{
    SlotType          = InSlot;
    bIsConsumableSlot = false;
    ConsumableIndex   = INDEX_NONE;
}

void UEquipmentSlotWidget::InitAsConsumableSlot(int32 InConsumableIndex)
{
    SlotType          = EEquipmentSlot::None;
    bIsConsumableSlot = true;
    ConsumableIndex   = InConsumableIndex;
}

void UEquipmentSlotWidget::SetEquippedItem(UItemDefinition* ItemDef)
{
    CurrentItem = ItemDef;

    if (ItemDef)
    {
        BP_OnItemEquipped(ItemDef, ItemDef->Icon);
    }
    else
    {
        BP_OnSlotCleared(SlotType, bIsConsumableSlot);
    }
}
