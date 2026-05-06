// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/ItemTypes.h"
#include "Types/UnitAttributeTypes.h"
#include "ItemDefinition.generated.h"

class AWorldItemActor;
class UStatusEffectDefinition;

// ---------------------------------------------------------------------------
// EDerivedCombatStat
//
// Identifies a derived combat stat on FBattleUnitState that an item can
// modify directly — bypassing the attribute formula.
// Example: a weapon that gives +10 PhysicalAttack regardless of STR.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EDerivedCombatStat : uint8
{
    MaxHP               UMETA(DisplayName = "Max HP"),
    PhysicalAttack      UMETA(DisplayName = "Physical Attack"),
    RangedAttack        UMETA(DisplayName = "Ranged Attack"),
    MagicalPower        UMETA(DisplayName = "Magical Power"),
    PhysicalDefense     UMETA(DisplayName = "Physical Defense"),
    MagicalDefense      UMETA(DisplayName = "Magical Defense"),
    ActionPoints        UMETA(DisplayName = "Action Points"),
    MovementRange       UMETA(DisplayName = "Movement Range"),
    HitChanceModifier   UMETA(DisplayName = "Hit Chance Modifier"),
    EvasionModifier     UMETA(DisplayName = "Evasion Modifier"),
    CritChance          UMETA(DisplayName = "Crit Chance"),
};

// ---------------------------------------------------------------------------
// FItemStatModifier
//
// A single stat bonus or penalty granted while this item is equipped.
// Supports two exclusive modes:
//   - Attribute mode (bModifiesAttribute = true):  adds to TemporaryAttributeBonuses
//     before the formula runs, affecting all stats derived from that attribute.
//   - Derived stat mode (bModifiesAttribute = false): adds a flat bonus directly
//     to the final FBattleUnitState field, bypassing the attribute formula.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FItemStatModifier
{
    GENERATED_BODY()

    // Human-readable label shown in item tooltip (e.g. "+5 Physical Attack").
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FText Label;

    // If true, modifies a base attribute (AttributeTarget).
    // If false, modifies a derived combat stat (DerivedStatTarget) directly.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bModifiesAttribute = false;

    // Which FUnitAttributes field to modify. Only used when bModifiesAttribute = true.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        meta = (EditCondition = "bModifiesAttribute", EditConditionHides))
    EUnitAttribute AttributeTarget = EUnitAttribute::Strength;

    // Which FBattleUnitState field to modify. Only used when bModifiesAttribute = false.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        meta = (EditCondition = "!bModifiesAttribute", EditConditionHides))
    EDerivedCombatStat DerivedStatTarget = EDerivedCombatStat::PhysicalAttack;

    // Flat integer delta applied to the target. Negative values reduce the stat.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 FlatBonus = 0;
};

// ---------------------------------------------------------------------------
// EConsumableEffectType
//
// The type of effect a consumable item applies when used.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EConsumableEffectType : uint8
{
    HealHP          UMETA(DisplayName = "Heal HP"),
    RestoreAP       UMETA(DisplayName = "Restore Action Points"),
    ApplyStatus     UMETA(DisplayName = "Apply Status Effect"),
    RemoveStatus    UMETA(DisplayName = "Remove Status Effect"),
    ReviveUnit      UMETA(DisplayName = "Revive Unit"),
};

// ---------------------------------------------------------------------------
// FConsumableEffect
//
// Defines one effect applied when a consumable item is used.
// A single item can have multiple effects (e.g. heal + apply a buff).
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FConsumableEffect
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
    EConsumableEffectType EffectType = EConsumableEffectType::HealHP;

    // Flat amount for HP heal, AP restore, or HP on revive.
    // Ignored by ApplyStatus / RemoveStatus.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect",
        meta = (EditCondition = "EffectType == EConsumableEffectType::HealHP || EffectType == EConsumableEffectType::RestoreAP || EffectType == EConsumableEffectType::ReviveUnit",
                EditConditionHides, ClampMin = "0"))
    int32 FlatValue = 0;

    // Status effect applied or removed.
    // Only used by ApplyStatus and RemoveStatus.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect",
        meta = (EditCondition = "EffectType == EConsumableEffectType::ApplyStatus || EffectType == EConsumableEffectType::RemoveStatus",
                EditConditionHides))
    TSoftObjectPtr<UStatusEffectDefinition> StatusEffect;
};

// ---------------------------------------------------------------------------
// UItemDefinition
//
// DataAsset that fully describes one item type. All items in the game are
// instances of this class; the shared inventory holds soft pointers to these
// assets alongside a quantity.
//
// Naming convention:  DA_Item_<Category>_<Name>
// Example:            DA_Item_Equipment_SteelArmor
// ---------------------------------------------------------------------------
UCLASS(BlueprintType)
class NEVERGONE_API UItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:

    // -----------------------------------------------------------------------
    // Identity
    // -----------------------------------------------------------------------

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FText Description;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    TObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    EItemCategory Category = EItemCategory::Material;

    // -----------------------------------------------------------------------
    // Economy
    // -----------------------------------------------------------------------

    // Base gold value used by the "Gold Value" sort filter and shop pricing.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "0"))
    int32 GoldValue = 0;

    // -----------------------------------------------------------------------
    // Inventory
    // -----------------------------------------------------------------------

    // How many of this item can occupy one inventory slot.
    // Set to 1 for unique gear (e.g. Steel Armor), up to 99 for consumables.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1", ClampMax = "999"))
    int32 MaxStackSize = 1;

    // Weight of a single unit of this item, in abstract weight units.
    // Contributes to the party's total carried weight against MaxWeight.
    // Equipped items do NOT count toward carried weight.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0.0"))
    float WeightPerUnit = 1.0f;

    // If true, the player cannot remove this item from the inventory via Drop.
    // Used for quest-critical items.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
    bool bIsKeyItem = false;

    // -----------------------------------------------------------------------
    // Equipment (only relevant when Category == EItemCategory::Equipment)
    // -----------------------------------------------------------------------

    // Which character slot this item occupies when equipped.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment",
        meta = (EditCondition = "Category == EItemCategory::Equipment"))
    EEquipmentSlot EquipmentSlot = EEquipmentSlot::None;

    // Classes that may equip this item. Empty array = no restriction (any class can equip).
    // Example: a Heavy Armor may only allow Towerborn and TorturedSavage.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    TArray<EUnitClass> AllowedClasses;

    // Stat modifiers applied to the equipping character while this item is worn.
    // Removed when the item is unequipped or swapped out.
    // Supports both attribute bonuses (pre-formula) and direct derived stat bonuses (post-formula).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    TArray<FItemStatModifier> StatModifiers;

    // Visual mesh attached to the character's socket when this item is equipped.
    // Null = no visual change (e.g. a necklace with no visible model).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Visual")
    TSoftObjectPtr<USkeletalMesh> EquipMesh;

    // Socket on the character's skeleton where EquipMesh is attached.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Visual")
    FName AttachSocketName = NAME_None;

    // -----------------------------------------------------------------------
    // Consumable (only relevant when Category == EItemCategory::Consumable)
    // -----------------------------------------------------------------------

    // Effects applied when this item is used. Executed in order.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable",
        meta = (EditCondition = "Category == EItemCategory::Consumable"))
    TArray<FConsumableEffect> ConsumableEffects;

    // -----------------------------------------------------------------------
    // Drop behavior
    // -----------------------------------------------------------------------

    // How many level visits a dropped instance of this item survives before
    // being removed. Default is 1: the item persists until the player leaves,
    // then disappears on the next visit. Set to 0 to make drops permanent.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "0"))
    int32 DropVisitLifetime = 2;

    // The AWorldItemActor Blueprint subclass that represents this item as a
    // physical pickup in the world. Used both when dropping from inventory and
    // when spawning loot from other sources (chests, rewards, etc.).
    // Must point to a BP that has a PickupMesh assigned.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drop")
    TSoftClassPtr<AWorldItemActor> WorldActorClass;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    // Returns true if the given class is allowed to equip this item.
    // Always returns true when AllowedClasses is empty (no restriction).
    bool CanBeEquippedByClass(EUnitClass UnitClass) const
    {
        if (AllowedClasses.IsEmpty()) return true;
        return AllowedClasses.Contains(UnitClass);
    }

    // Returns true if this item can be placed in the consumable hotbar
    // (only EItemCategory::Consumable items may occupy combat consumable slots).
    bool IsCombatConsumable() const
    {
        return Category == EItemCategory::Consumable;
    }
};
