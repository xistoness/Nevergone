// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "ItemTypes.generated.h"

class UItemDefinition;

// ---------------------------------------------------------------------------
// EUnitClass
//
// Canonical enum for all playable and enemy unit classes in Nevergone.
// Used by UUnitDefinition::UnitClass and UItemDefinition::AllowedClasses
// to enforce equipment restrictions without string comparisons.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EUnitClass : uint8
{
    None            UMETA(DisplayName = "None"),
    Planewalker     UMETA(DisplayName = "Planewalker"),
    Defiled         UMETA(DisplayName = "Defiled"),
    TorturedSavage  UMETA(DisplayName = "Tortured Savage"),
    Towerborn       UMETA(DisplayName = "Towerborn"),
};

// ---------------------------------------------------------------------------
// EItemCategory
//
// Broad classification for items. Drives inventory filter grouping and UI
// icon badges. Consumable = usable from menu; KeyItem = never droppable.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EItemCategory : uint8
{
    Equipment   UMETA(DisplayName = "Equipment"),
    Consumable  UMETA(DisplayName = "Consumable"),
    Material    UMETA(DisplayName = "Material"),
    KeyItem     UMETA(DisplayName = "Key Item"),

    // Currency items never occupy a slot in the player inventory. When picked
    // up or transferred from a container, UInventorySubsystem intercepts and
    // increments FPartyData::Gold instead of creating an FInventoryEntry.
    // Containers (chests) still hold them as normal entries — the conversion
    // is one-way at the moment of transfer-into-player.
    Currency    UMETA(DisplayName = "Currency"),
};

// ---------------------------------------------------------------------------
// EEquipmentSlot
//
// Identifies which of the four equipment slots a piece of gear occupies.
// Items with EItemCategory::Equipment must declare a slot; the inventory
// system uses this to route the item into the correct FEquipmentLoadout field.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EEquipmentSlot : uint8
{
    None        UMETA(DisplayName = "None"),
    Armor       UMETA(DisplayName = "Armor"),
    Weapon      UMETA(DisplayName = "Weapon"),
    Necklace    UMETA(DisplayName = "Necklace"),
    Belt        UMETA(DisplayName = "Belt"),
};

// ---------------------------------------------------------------------------
// EInventorySortField
//
// Determines which property is used as the primary sort key for one sort layer.
// Multiple layers are stacked in order; ties in layer N fall through to layer N+1.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EInventorySortField : uint8
{
    None        UMETA(DisplayName = "None"),
    Name        UMETA(DisplayName = "Name (A→Z)"),
    Category    UMETA(DisplayName = "Category"),
    DateAdded   UMETA(DisplayName = "Date Added"),
    GoldValue   UMETA(DisplayName = "Gold Value"),
    Weight      UMETA(DisplayName = "Weight"),
};

// ---------------------------------------------------------------------------
// EInventorySortOrder
//
// Direction applied to a single EInventorySortField layer.
// Ascending = A→Z / low→high / oldest→newest.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EInventorySortOrder : uint8
{
    Ascending   UMETA(DisplayName = "Ascending"),
    Descending  UMETA(DisplayName = "Descending"),
};

// ---------------------------------------------------------------------------
// FInventorySortLayer
//
// One level in a multi-layered sort stack.
// The inventory sorts by Layers[0] first; ties break by Layers[1], and so on.
// Saved to disk so the player's preferred sort order persists across sessions.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FInventorySortLayer
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite)
    EInventorySortField Field = EInventorySortField::None;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite)
    EInventorySortOrder Order = EInventorySortOrder::Ascending;
};

// ---------------------------------------------------------------------------
// FInventoryEntry
//
// Runtime record for one stack of items in the shared inventory.
// Uses a soft pointer so the DataAsset is not loaded until accessed.
// AcquiredTimestamp drives the "Date Added" sort field.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FInventoryEntry
{
    GENERATED_BODY()

    // Soft pointer to the DataAsset that describes this item type.
    // Resolved lazily via LoadSynchronous() when the item needs to be inspected.
    UPROPERTY(SaveGame, BlueprintReadWrite)
    TSoftObjectPtr<UItemDefinition> Definition;

    // Number of items in this stack. Always >= 1 and <= UItemDefinition::MaxStackSize.
    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 Quantity = 1;

    // UTC timestamp recorded when the item first entered the inventory.
    // Used for DateAdded sort ordering.
    UPROPERTY(SaveGame, BlueprintReadWrite)
    FDateTime AcquiredTimestamp;

    bool IsValid() const { return !Definition.IsNull() && Quantity > 0; }
};

// ---------------------------------------------------------------------------
// FEquipmentLoadout
//
// Per-character equipment state: four gear slots + four combat consumable slots.
// Replaces the old TArray<FName> EquippedItems in FPartyMemberData.
//
// Equipment slots are null when empty.
// CombatConsumables[0..3] map to the four hotbar slots shown during battle.
// Items moved into any slot are removed from the shared inventory.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FEquipmentLoadout
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Equipment")
    TSoftObjectPtr<UItemDefinition> Armor;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Equipment")
    TSoftObjectPtr<UItemDefinition> Weapon;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Equipment")
    TSoftObjectPtr<UItemDefinition> Necklace;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Equipment")
    TSoftObjectPtr<UItemDefinition> Belt;

    // Four slots for consumable items usable during combat via the "Use Item" ability.
    // Fixed size of 4; empty slots are null.
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Consumables")
    TArray<TSoftObjectPtr<UItemDefinition>> CombatConsumables;

    FEquipmentLoadout()
    {
        CombatConsumables.SetNum(4);
    }

    // Returns the item in the given equipment slot, or null if empty.
    TSoftObjectPtr<UItemDefinition> GetSlot(EEquipmentSlot Slot) const
    {
        switch (Slot)
        {
        case EEquipmentSlot::Armor:    return Armor;
        case EEquipmentSlot::Weapon:   return Weapon;
        case EEquipmentSlot::Necklace: return Necklace;
        case EEquipmentSlot::Belt:     return Belt;
        default:                       return nullptr;
        }
    }

    // Writes ItemDef into the given slot. Pass nullptr to clear the slot.
    void SetSlot(EEquipmentSlot Slot, TSoftObjectPtr<UItemDefinition> ItemDef)
    {
        switch (Slot)
        {
        case EEquipmentSlot::Armor:    Armor    = ItemDef; break;
        case EEquipmentSlot::Weapon:   Weapon   = ItemDef; break;
        case EEquipmentSlot::Necklace: Necklace = ItemDef; break;
        case EEquipmentSlot::Belt:     Belt     = ItemDef; break;
        default: break;
        }
    }
};
