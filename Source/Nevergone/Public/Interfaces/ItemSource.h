// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/ItemTypes.h"
#include "ItemSource.generated.h"

class UItemDefinition;

// Result codes are reused from EInventoryResult (declared in InventorySubsystem.h
// to avoid a circular include here, both sides share the enum).
enum class EInventoryResult : uint8;

UINTERFACE(MinimalAPI)
class UItemSource : public UInterface
{
    GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// IItemSource
//
// Agnostic boundary for any object that "holds items" — the player's shared
// inventory, a world container (chest, barrel), and (later) shop NPCs all
// implement it. UItemContainerSubsystem operates only against this interface,
// so a single transfer pipeline serves chests today and shops tomorrow.
//
// Lifetime: callers should never store IItemSource* across frames. Hold the
// underlying UObject (TWeakObjectPtr / TScriptInterface) and resolve to the
// interface when needed.
//
// All implementations are responsible for broadcasting their own change
// events (UInventorySubsystem::OnInventoryChanged, AContainerActor::OnContentsChanged)
// after ReceiveTransfer / ProvideTransfer. UItemContainerSubsystem does not
// broadcast on behalf of either side.
// ---------------------------------------------------------------------------
class NEVERGONE_API IItemSource
{
    GENERATED_BODY()

public:

    // Returns a snapshot of the entries this source currently holds.
    // The returned array is a copy — safe to iterate during mutations on the source.
    virtual TArray<FInventoryEntry> GetItemEntries() const = 0;

    // Maximum slot count this source can hold.
    virtual int32 GetMaxSlots() const = 0;

    // Number of slots currently in use.
    virtual int32 GetUsedSlots() const = 0;

    // True if Quantity units of ItemDef are currently present.
    virtual bool HasItem(const UItemDefinition* ItemDef, int32 Quantity) const = 0;

    // True if Quantity units of ItemDef can be added without exceeding capacity.
    // Implementations should account for stack merging (existing stacks with room)
    // before consuming a new slot. Weight rules are NOT enforced here — see
    // ItemContainerSubsystem comment on how the player inventory still allows
    // overweight transfers.
    virtual bool CanAcceptItem(const UItemDefinition* ItemDef, int32 Quantity) const = 0;

    // Adds Quantity units of ItemDef to this source as part of an incoming
    // transfer. Implementations decide their own semantics:
    //   - UInventorySubsystem allows overweight (the player accepts heavy
    //     items from chests and pays with the Overweight status afterwards).
    //   - AContainerActor only checks slot capacity.
    // Each implementation is responsible for broadcasting its own change event.
    virtual EInventoryResult ReceiveTransfer(UItemDefinition* ItemDef, int32 Quantity) = 0;

    // Removes Quantity units of ItemDef as part of an outgoing transfer.
    // Returns Success or FailedNotFound.
    virtual EInventoryResult ProvideTransfer(UItemDefinition* ItemDef, int32 Quantity) = 0;

    // Display label for the source side of a transfer UI ("Inventory", "Wooden Chest").
    virtual FText GetSourceDisplayName() const = 0;
};
