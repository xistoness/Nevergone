// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ContainerDefinition.generated.h"

class ULootTable;

// ---------------------------------------------------------------------------
// UContainerDefinition
//
// Reusable DataAsset describing a "container archetype" — e.g. WoodenChest,
// MetalCofre, BarrelOfTheCellar. Each AContainerActor placed in a level
// references one of these to share rules with every other container of the
// same type.
//
// Containers in Nevergone do NOT enforce a weight limit (capacity is purely
// slot-count based). The AContainerActor still computes the total weight of
// its contents and exposes it for read-only display in the transfer UI.
//
// LootTable is optional. When set, it rolls once on the FIRST time the
// player opens the container — see AContainerActor::EnsureInitialized.
// Subsequent visits (before any save) re-open the same rolled state.
// Once the actor's bHasBeenLooted flag is persisted, the table never rolls
// again for that instance, which means later edits to the table do not
// retroactively affect already-discovered chests. That is intentional and
// matches typical RPG behaviour.
// ---------------------------------------------------------------------------
UCLASS(BlueprintType)
class NEVERGONE_API UContainerDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:

    // Player-facing name shown as the right-panel title in the transfer widget.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container")
    FText DisplayName;

    // Maximum slot count this container can hold. Each FInventoryEntry occupies
    // one slot regardless of stack size.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container",
              meta = (ClampMin = "1", ClampMax = "999"))
    int32 MaxSlots = 30;

    // Optional loot table rolled the first time a player opens an instance.
    // Leave null for player-only storage (cofre que começa vazio).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container")
    TSoftObjectPtr<ULootTable> LootTable;
};
