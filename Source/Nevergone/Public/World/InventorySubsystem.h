// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/ItemTypes.h"
#include "Data/PartyData.h"
#include "Interfaces/ItemSource.h"
#include "InventorySubsystem.generated.h"

class UItemDefinition;
class UStatusEffectDefinition;
class ACharacterBase;
class UMyGameInstance;
class USaveableComponent;
class AWorldItemActor;

// Result codes returned by inventory mutation operations.
// Callers use these to drive contextual feedback (e.g. "Inventory full" toast).
UENUM(BlueprintType)
enum class EInventoryResult : uint8
{
    Success,
    FailedInventoryFull,     // All 30 slots occupied and no matching stack to merge into
    FailedOverweight,        // Adding the item would exceed MaxWeight
    FailedStackFull,         // Item has MaxStackSize == 1 and a stack already exists
    FailedNotFound,          // Item not in inventory (remove/equip attempt)
    FailedClassRestriction,  // Character class cannot equip this item
    FailedWrongCategory,     // Trying to equip a non-equipment item into a gear slot
    FailedSlotOccupied,      // Consumable slot already holds an item (must clear first)
    FailedKeyItem,           // Key items cannot be dropped
};

// ---------------------------------------------------------------------------
// UInventorySubsystem
//
// GameInstanceSubsystem — single runtime authority for the shared party
// inventory and all per-character equipment loadouts.
//
// Lifecycle:
//   Initialize()     — called automatically by UE subsystem infrastructure
//   LoadFromSave()   — call after UMySaveGame is loaded; populates runtime state
//   FlushToSave()    — call before CommitSave(); writes runtime state back
//
// Weight model:
//   MaxWeight  = 10 + 5 * SumOfActivePartySTR
//   TotalWeight = sum of (entry.Quantity * entry.Definition->WeightPerUnit)
//   Equipped items do NOT count toward TotalWeight.
//
// Overweight handling:
//   When TotalWeight > MaxWeight, the Overweight status (DurationTurns==0) is
//   applied to all active party members via StatusEffectManager.
//   It is removed automatically when TotalWeight <= MaxWeight again.
//
// Sort model:
//   SortLayers is a player-set stack of up to 4 FInventorySortLayer entries.
//   ApplySort() executes a stable multi-key sort: layers are applied from last
//   to first so the first layer wins (std::stable_sort guarantee).
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UInventorySubsystem : public UGameInstanceSubsystem, public IItemSource
{
    GENERATED_BODY()

public:

    // -----------------------------------------------------------------------
    // Subsystem lifecycle
    // -----------------------------------------------------------------------

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Populates runtime state from the active save. Call after save is loaded.
    void LoadFromSave();

    // Writes runtime state back into the active save. Call before CommitSave().
    void FlushToSave();

    // -----------------------------------------------------------------------
    // Inventory mutations
    // -----------------------------------------------------------------------

    /**
     * Attempts to add Quantity units of ItemDef to the shared inventory.
     * Merges into an existing stack if one exists and has room.
     * Opens a new slot if no matching stack is found.
     * Fails if all 30 slots are occupied or carrying the item would exceed MaxWeight.
     *
     * @param ItemDef   DataAsset describing the item type. Must not be null.
     * @param Quantity  Number of units to add. Must be >= 1.
     * @return          EInventoryResult::Success or a failure code.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    EInventoryResult AddItem(UItemDefinition* ItemDef, int32 Quantity = 1);

    /**
     * Removes Quantity units of ItemDef from the shared inventory.
     * If Quantity equals or exceeds the stack size, the slot is freed.
     *
     * @param ItemDef   DataAsset identifying the item to remove.
     * @param Quantity  Units to remove. Clamped to available stack size.
     * @return          EInventoryResult::Success or FailedNotFound.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    EInventoryResult RemoveItem(UItemDefinition* ItemDef, int32 Quantity = 1);

    /**
     * Equips ItemDef from the shared inventory into CharacterID's matching slot.
     * If a different item already occupies the slot it is swapped back to inventory.
     * Removes the equipped item's weight from the total and applies stat modifiers.
     *
     * @param CharacterID   GUID identifying the party member (from FPartyMemberData).
     * @param ItemDef       Item to equip. Must be EItemCategory::Equipment.
     * @return              EInventoryResult code.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    EInventoryResult EquipItem(const FGuid& CharacterID, UItemDefinition* ItemDef);

    /**
     * Removes the item in Slot from CharacterID's loadout back into the inventory.
     * Reverses stat modifiers and re-adds the item's weight to the total.
     *
     * @param CharacterID   GUID of the target party member.
     * @param Slot          Which equipment slot to clear.
     * @return              EInventoryResult code.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    EInventoryResult UnequipItem(const FGuid& CharacterID, EEquipmentSlot Slot);

    /**
     * Assigns ItemDef to a combat consumable slot on CharacterID's loadout.
     * The item is removed from the shared inventory immediately.
     *
     * @param CharacterID   GUID of the target party member.
     * @param SlotIndex     Consumable slot index [0..3].
     * @param ItemDef       Must be EItemCategory::Consumable.
     * @return              EInventoryResult code.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    EInventoryResult EquipConsumable(const FGuid& CharacterID, int32 SlotIndex, UItemDefinition* ItemDef);

    /**
     * Returns the consumable at SlotIndex for CharacterID to the shared inventory.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    EInventoryResult UnequipConsumable(const FGuid& CharacterID, int32 SlotIndex);

    /**
     * Drops one unit of ItemDef from the inventory, spawning an ADroppedItemActor
     * at SpawnLocation in the current world. Fails for key items.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    EInventoryResult DropItem(UItemDefinition* ItemDef, const FVector& SpawnLocation);

    /**
     * Spawns ItemDef's pickup actor (its WorldActorClass) as a dropped item,
     * without touching the inventory. Used by DropItem and by thrown
     * distractions coming to rest.
     */
    AWorldItemActor* SpawnWorldItem(UItemDefinition* ItemDef, const FVector& Location,
        const FRotator& Rotation = FRotator::ZeroRotator);

    // -----------------------------------------------------------------------
    // Weight queries
    // -----------------------------------------------------------------------

    // Returns the maximum weight the active party can carry.
    // Formula: 10 + 5 * (sum of Strength from all active members).
    UFUNCTION(BlueprintPure, Category = "Inventory|Weight")
    float GetMaxWeight() const;

    // Returns the current total weight of all items in the shared inventory.
    // Equipped items are excluded.
    UFUNCTION(BlueprintPure, Category = "Inventory|Weight")
    float GetCurrentWeight() const;

    // Returns true when GetCurrentWeight() > GetMaxWeight().
    UFUNCTION(BlueprintPure, Category = "Inventory|Weight")
    bool IsOverweight() const;

    // -----------------------------------------------------------------------
    // Gold (party currency)
    //
    // Currency items (Category == EItemCategory::Currency) never occupy an
    // inventory slot. They are intercepted by AddItem and ReceiveTransfer,
    // which forward to AddGold(). Direct callers (quest rewards, debug menus)
    // should also use AddGold / SpendGold so OnGoldChanged fires.
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintPure, Category = "Inventory|Gold")
    int32 GetGold() const;

    // Adds Amount to the party's gold counter. Negative or zero amounts are
    // ignored with a warning. Always succeeds (no upper bound).
    UFUNCTION(BlueprintCallable, Category = "Inventory|Gold")
    void AddGold(int32 Amount);

    // Removes Amount from the party's gold counter if available.
    // Returns true on success, false if the player did not have enough.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Gold")
    bool SpendGold(int32 Amount);

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoldChanged, int32 /*NewGold*/);
    FOnGoldChanged OnGoldChanged;

    // -----------------------------------------------------------------------
    // Sort
    // -----------------------------------------------------------------------

    // Returns the current sort stack (max 4 layers).
    UFUNCTION(BlueprintPure, Category = "Inventory|Sort")
    const TArray<FInventorySortLayer>& GetSortLayers() const { return SortLayers; }

    // Replaces the sort stack and immediately re-sorts the inventory.
    // Also marks the save dirty so SortLayers persist.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Sort")
    void SetSortLayers(const TArray<FInventorySortLayer>& NewLayers);

    // Re-applies the current SortLayers to RuntimeInventory.
    // Called automatically after any mutation and after SetSortLayers().
    UFUNCTION(BlueprintCallable, Category = "Inventory|Sort")
    void ApplySort();

    // -----------------------------------------------------------------------
    // Read access
    // -----------------------------------------------------------------------

    // Returns a read-only view of the sorted inventory entries.
    UFUNCTION(BlueprintPure, Category = "Inventory")
    const TArray<FInventoryEntry>& GetInventory() const { return RuntimeInventory; }

    // Returns the inventory entry for ItemDef, or nullptr if not found.
    const FInventoryEntry* FindEntry(const UItemDefinition* ItemDef) const;

    // Returns true if the inventory already contains at least one unit of ItemDef.
    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool HasItem(UItemDefinition* ItemDef) const;

    // Returns the FEquipmentLoadout for CharacterID, or nullptr if not found.
    FEquipmentLoadout* GetLoadoutForCharacter(const FGuid& CharacterID);

    // -----------------------------------------------------------------------
    // Delegates — broadcast after successful mutations so widgets can refresh
    // -----------------------------------------------------------------------

    DECLARE_MULTICAST_DELEGATE(FOnInventoryChanged);
    FOnInventoryChanged OnInventoryChanged;

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnWeightChanged, float /*NewWeight*/);
    FOnWeightChanged OnWeightChanged;

    // -----------------------------------------------------------------------
    // IItemSource implementation
    //
    // Used by UItemContainerSubsystem to make the player's shared inventory
    // a participant in agnostic transfers (chest <-> player today, shop <->
    // player tomorrow). ReceiveTransfer here intentionally bypasses the
    // weight ceiling — chest-to-player transfers are allowed to push the
    // player into overweight; the Overweight status, applied automatically
    // by RecalculateWeightAndUpdateStatus, is the player's feedback.
    // -----------------------------------------------------------------------

    virtual TArray<FInventoryEntry> GetItemEntries() const override { return RuntimeInventory; }
    virtual int32 GetMaxSlots() const override { return MaxInventorySlots; }
    virtual int32 GetUsedSlots() const override { return RuntimeInventory.Num(); }
    virtual bool HasItem(const UItemDefinition* ItemDef, int32 Quantity) const override;
    virtual bool CanAcceptItem(const UItemDefinition* ItemDef, int32 Quantity) const override;
    virtual EInventoryResult ReceiveTransfer(UItemDefinition* ItemDef, int32 Quantity) override;
    virtual EInventoryResult ProvideTransfer(UItemDefinition* ItemDef, int32 Quantity) override;
    virtual FText GetSourceDisplayName() const override;

    // -----------------------------------------------------------------------
    // Overweight status integration
    // -----------------------------------------------------------------------

    // DataAsset for the Overweight status effect.
    // Cannot be EditDefaultsOnly on a UGameInstanceSubsystem (UE limitation — subsystem
    // Class Defaults are not editable in the editor). Assign this from UMyGameInstance
    // after the subsystem initializes, e.g. in a BP override of Init() or via a
    // dedicated config property on the GameInstance BP.
    UPROPERTY(BlueprintReadWrite, Category = "Inventory|Overweight")
    TSoftObjectPtr<UStatusEffectDefinition> OverweightStatusDefinition;

private:

    // Runtime copy of SharedInventory — the single source of truth in-memory.
    // Sorted after every mutation.
    TArray<FInventoryEntry> RuntimeInventory;

    // Runtime copy of InventorySortLayers.
    TArray<FInventorySortLayer> SortLayers;

    // Cached overweight state — used to avoid re-applying the status every frame.
    bool bWasOverweight = false;

    // Maximum number of inventory slots the party can fill.
    static constexpr int32 MaxInventorySlots = 30;

    // Maximum number of sort layers.
    static constexpr int32 MaxSortLayers = 4;

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    // Recalculates TotalWeight, fires OnWeightChanged, and applies/removes
    // the Overweight status on all active party members if the state changed.
    void RecalculateWeightAndUpdateStatus();

    // Applies stat modifiers from ItemDef to the CharacterBase actor matching CharacterID.
    // Pass bApply=false to reverse (unequip) the modifiers.
    void ApplyEquipmentModifiers(const FGuid& CharacterID, const UItemDefinition* ItemDef, bool bApply);

    // Returns the ACharacterBase* for CharacterID from the current world, or nullptr.
    ACharacterBase* FindCharacterActor(const FGuid& CharacterID) const;

    // Returns the FPartyMemberData* for CharacterID from the active save, or nullptr.
    FPartyMemberData* FindPartyMember(const FGuid& CharacterID) const;

    // Compares two FInventoryEntry instances for a single sort field.
    // Returns negative / zero / positive (like strcmp).
    int32 CompareEntries(const FInventoryEntry& A, const FInventoryEntry& B, EInventorySortField Field) const;

    UMyGameInstance* GetGameInstance() const;
};
