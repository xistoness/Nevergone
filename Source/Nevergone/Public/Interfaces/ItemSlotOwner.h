// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemSlotOwner.generated.h"

// ---------------------------------------------------------------------------
// EContainerPanel
//
// Identifies which side of the split layout the cursor is currently on.
// Left  = player inventory (6 cols x 5 rows = 30 slots).
// Right = container (grid sized from UContainerDefinition::MaxSlots).
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EContainerPanel : uint8
{
    PlayerInventory   UMETA(DisplayName = "Player Inventory"),
    Container         UMETA(DisplayName = "Container"),
};

class UWidget;

UINTERFACE(MinimalAPI, BlueprintType)
class UItemSlotOwner : public UInterface
{
    GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// IItemSlotOwner
//
// Implemented by any widget that owns a list of UItemSlotWidget children —
// the player inventory screen and the container transfer screen today,
// future shop screens tomorrow. Lets UItemSlotWidget call back into its
// owner without knowing the concrete widget class.
//
// Why an interface and not a base UUserWidget class:
//   UInventoryWidget already exists with a fully-skinned BP child, and
//   UContainerTransferWidget has a different UI shape. Forcing both to
//   share a base would inflate that base with code only one of them needs.
//   The interface only declares the click-callback shape — each owner
//   implements it however its layout demands.
//
// Implementation pattern:
//   UCLASS()
//   class UMyWidget : public UUserWidget, public IItemSlotOwner
//   {
//       virtual void HandleSlotClicked_Implementation(int32 SlotIndex, UWidget* OriginWidget) override;
//   };
//
// Slot widgets dispatch via:
//   IItemSlotOwner::Execute_HandleSlotClicked(OwnerObj.GetObject(), SlotIndex, OriginWidget);
// ---------------------------------------------------------------------------
class NEVERGONE_API IItemSlotOwner
{
    GENERATED_BODY()

public:

    // Fired by an UItemSlotWidget when the player clicks (or presses confirm
    // while focused on) its slot. SlotIndex is the slot's position in the
    // owner's grid — owner decides what to do with it. OriginWidget is the
    // clicked button widget; owners typically read its geometry to anchor a
    // context menu next to the slot.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ItemSlot")
    void HandleSlotClicked(int32 SlotIndex, UWidget* OriginWidget);
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ItemSlot")
    void HandlePanelSlotClicked(EContainerPanel OwningPanel, int32 SlotIndex, UWidget* OriginWidget);
};
