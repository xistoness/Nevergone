// Copyright Xyzto Works

#include "Widgets/ItemSlotWidget.h"
#include "Data/ItemDefinition.h"

void UItemSlotWidget::SetSlotData(int32 InSlotIndex, const FInventoryEntry& InEntry)
{
    SlotIndex    = InSlotIndex;
    CurrentEntry = InEntry;

    const UItemDefinition* Def = InEntry.Definition.Get();
    UTexture2D* Icon           = Def ? Def->Icon : nullptr;
    const float Weight         = Def ? Def->WeightPerUnit * InEntry.Quantity : 0.f;

    UE_LOG(LogTemp, Log, TEXT("[ItemSlotWidget] SetSlotData slot %d — '%s'."),
        InSlotIndex, Def ? *Def->DisplayName.ToString() : TEXT("null"));

    BP_OnSlotPopulated(InEntry, Icon, InEntry.Quantity, Weight);
}

void UItemSlotWidget::ClearSlot(int32 InSlotIndex)
{
    UE_LOG(LogTemp, Log, TEXT("[ItemSlotWidget] ClearSlot %d."), InSlotIndex);

    SlotIndex    = InSlotIndex;
    CurrentEntry = FInventoryEntry();
    BP_OnSlotCleared();
}