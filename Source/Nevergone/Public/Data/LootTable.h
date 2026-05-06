// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/ItemTypes.h"
#include "LootTable.generated.h"

class UItemDefinition;

// ---------------------------------------------------------------------------
// FLootEntry
//
// One row of a loot table. Rolls happen independently per row — each entry's
// DropChance is rolled separately, then on success a quantity is rolled
// uniformly from [MinQuantity, MaxQuantity]. There are no "weighted picks"
// or "guaranteed N drops" — keep it simple. Add those when actually needed.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FLootEntry
{
    GENERATED_BODY()

    // Item that may drop.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
    TSoftObjectPtr<UItemDefinition> ItemDef;

    // Probability of dropping on a single roll. Range [0..1].
    // 1.0 = always drops; 0.0 = never drops (use to temporarily disable an entry).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DropChance = 1.0f;

    // Inclusive range for the quantity dropped on a successful roll.
    // For unique items (MaxStackSize == 1), use 1..1.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot",
              meta = (ClampMin = "1"))
    int32 MinQuantity = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot",
              meta = (ClampMin = "1"))
    int32 MaxQuantity = 1;
};

// ---------------------------------------------------------------------------
// FRolledLoot
//
// Result of one ULootTable::Roll() call — list of (item, quantity) pairs ready
// to be added to a container's inventory.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FRolledLoot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Loot")
    TObjectPtr<UItemDefinition> ItemDef = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Loot")
    int32 Quantity = 0;
};

// ---------------------------------------------------------------------------
// ULootTable
//
// Reusable DataAsset describing the items that may spawn in a container the
// first time it is opened. Same instance can be referenced by many containers
// ("Loot_Forest_Common" used by every chest in the forest biome).
//
// Roll() is deterministic only with respect to FMath::RandRange — there is no
// seeding. If reproducible loot is ever needed (speedrun mode, tests), pass
// an FRandomStream from the caller.
// ---------------------------------------------------------------------------
UCLASS(BlueprintType)
class NEVERGONE_API ULootTable : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:

    // Rows rolled independently in order.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
    TArray<FLootEntry> Entries;

    // Rolls every entry once and returns the items that landed.
    // Empty array if no entry succeeded or the table itself is empty.
    UFUNCTION(BlueprintCallable, Category = "Loot")
    TArray<FRolledLoot> Roll() const;
};
