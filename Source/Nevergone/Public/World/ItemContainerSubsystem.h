// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "World/InventorySubsystem.h" // For EInventoryResult
#include "ItemContainerSubsystem.generated.h"

class IItemSource;
class UItemDefinition;
class UObject;

// ---------------------------------------------------------------------------
// UItemContainerSubsystem
//
// Agnostic transfer pipeline. Operates on any pair of IItemSource — the
// player's UInventorySubsystem and an AContainerActor today, an NPC shop's
// stock + the player tomorrow. Validates capacity, does the swap
// transactionally (rolls back on add-failure), and routes change events
// through each source's own delegate.
//
// Containers are the only side of a transfer that may push a player into
// overweight: the player's UInventorySubsystem allows AddItem unconditionally
// (it does not enforce a weight ceiling), and the transfer UI already
// surfaces the resulting overweight state visually. Slot capacity, on the
// other hand, IS enforced — a transfer that would not fit is blocked.
//
// This subsystem is stateless; safe to call from any thread that already
// has GameInstance access.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UItemContainerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    /**
     * Moves Quantity units of ItemDef from Source to Dest.
     *
     * Both pointers must be UObjects that implement IItemSource — typically
     * UInventorySubsystem (the player) and AContainerActor (a chest).
     *
     * Validation order:
     *   1. Source has the item        → FailedNotFound
     *   2. Dest can accept the item   → FailedInventoryFull
     *   3. Remove from source         → propagated failure code
     *   4. Add to dest                → on failure, re-add to source and
     *                                   propagate the failure code.
     *
     * Each side broadcasts its own change event from inside Add/Remove —
     * this subsystem does not broadcast on behalf of either party.
     *
     * @param Source     UObject implementing IItemSource (the giver).
     * @param Dest       UObject implementing IItemSource (the receiver).
     * @param ItemDef    Item to move. Must not be null.
     * @param Quantity   Units to move. Must be >= 1.
     * @return           EInventoryResult::Success or a failure code.
     */
    EInventoryResult TransferItem(UObject* Source,
                                  UObject* Dest,
                                  UItemDefinition* ItemDef,
                                  int32 Quantity);

private:

    // Resolves a UObject* to an IItemSource* — null if the object does not
    // implement the interface.
    IItemSource* ResolveSource(UObject* Obj) const;
};
