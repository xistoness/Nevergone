// Copyright Xyzto Works

#include "Data/LootTable.h"
#include "Data/ItemDefinition.h"

TArray<FRolledLoot> ULootTable::Roll() const
{
    TArray<FRolledLoot> Result;

    if (Entries.Num() == 0)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[LootTable] '%s' has no entries — empty roll."),
            *GetName());
        return Result;
    }

    for (const FLootEntry& Entry : Entries)
    {
        if (Entry.ItemDef.IsNull())
        {
            UE_LOG(LogTemp, Warning, TEXT("[LootTable] '%s' entry has null ItemDef — skipping."),
                *GetName());
            continue;
        }

        // FMath::FRand returns [0,1). DropChance >= 1 always passes.
        const float Roll = FMath::FRand();
        if (Roll > Entry.DropChance)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[LootTable] '%s' miss '%s' (roll=%.2f > chance=%.2f)."),
                *GetName(),
                *Entry.ItemDef.GetAssetName(),
                Roll, Entry.DropChance);
            continue;
        }

        const int32 Min = FMath::Max(1, Entry.MinQuantity);
        const int32 Max = FMath::Max(Min, Entry.MaxQuantity);
        const int32 Quantity = FMath::RandRange(Min, Max);

        UItemDefinition* LoadedItem = Entry.ItemDef.LoadSynchronous();
        if (!LoadedItem)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LootTable] '%s' failed to load ItemDef '%s'."),
                *GetName(), *Entry.ItemDef.ToSoftObjectPath().ToString());
            continue;
        }

        FRolledLoot Drop;
        Drop.ItemDef  = LoadedItem;
        Drop.Quantity = Quantity;
        Result.Add(Drop);

        UE_LOG(LogTemp, Log, TEXT("[LootTable] '%s' hit '%s' x%d."),
            *GetName(), *LoadedItem->DisplayName.ToString(), Quantity);
    }

    return Result;
}
