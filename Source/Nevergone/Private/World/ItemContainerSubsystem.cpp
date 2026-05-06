// Copyright Xyzto Works

#include "World/ItemContainerSubsystem.h"
#include "Interfaces/ItemSource.h"
#include "Data/ItemDefinition.h"

EInventoryResult UItemContainerSubsystem::TransferItem(
    UObject*         Source,
    UObject*         Dest,
    UItemDefinition* ItemDef,
    int32            Quantity)
{
    if (!ItemDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ItemContainerSubsystem] TransferItem: null ItemDef."));
        return EInventoryResult::FailedNotFound;
    }
    if (Quantity <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ItemContainerSubsystem] TransferItem: non-positive quantity %d."),
            Quantity);
        return EInventoryResult::FailedNotFound;
    }

    IItemSource* Src = ResolveSource(Source);
    IItemSource* Dst = ResolveSource(Dest);
    if (!Src || !Dst)
    {
        UE_LOG(LogTemp, Error, TEXT("[ItemContainerSubsystem] TransferItem: source or dest does not implement IItemSource."));
        return EInventoryResult::FailedNotFound;
    }
    if (Src == Dst)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ItemContainerSubsystem] TransferItem: source == dest, no-op."));
        return EInventoryResult::Success;
    }

    UE_LOG(LogTemp, Log, TEXT("[ItemContainerSubsystem] TransferItem: '%s' x%d from '%s' -> '%s'."),
        *ItemDef->DisplayName.ToString(),
        Quantity,
        *Src->GetSourceDisplayName().ToString(),
        *Dst->GetSourceDisplayName().ToString());

    // 1. Source has it
    if (!Src->HasItem(ItemDef, Quantity))
    {
        UE_LOG(LogTemp, Log, TEXT("[ItemContainerSubsystem] TransferItem blocked: source lacks '%s' x%d."),
            *ItemDef->DisplayName.ToString(), Quantity);
        return EInventoryResult::FailedNotFound;
    }

    // 2. Dest has room
    if (!Dst->CanAcceptItem(ItemDef, Quantity))
    {
        UE_LOG(LogTemp, Log, TEXT("[ItemContainerSubsystem] TransferItem blocked: dest cannot fit '%s' x%d."),
            *ItemDef->DisplayName.ToString(), Quantity);
        return EInventoryResult::FailedInventoryFull;
    }

    // 3. Remove from source
    const EInventoryResult RemoveResult = Src->ProvideTransfer(ItemDef, Quantity);
    if (RemoveResult != EInventoryResult::Success)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ItemContainerSubsystem] TransferItem: source provide failed (code=%d)."),
            (int32)RemoveResult);
        return RemoveResult;
    }

    // 4. Add to dest, rolling back on failure.
    const EInventoryResult AddResult = Dst->ReceiveTransfer(ItemDef, Quantity);
    if (AddResult != EInventoryResult::Success)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ItemContainerSubsystem] TransferItem: dest receive failed (code=%d) — rolling back."),
            (int32)AddResult);
        Src->ReceiveTransfer(ItemDef, Quantity);
        return AddResult;
    }

    return EInventoryResult::Success;
}

IItemSource* UItemContainerSubsystem::ResolveSource(UObject* Obj) const
{
    return Cast<IItemSource>(Obj);
}
