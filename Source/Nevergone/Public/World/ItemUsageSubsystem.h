// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ItemUsageSubsystem.generated.h"

class UItemDefinition;
class ACharacterBase;
class UMyGameInstance;
class UPartyManagerSubsystem;
struct FConsumableEffect;
struct FPartyMemberData;

// ---------------------------------------------------------------------------
// EItemUseSource
//
// Identifies where the consumed unit of the item is taken from.
//   - Inventory:           the shared inventory stack — decrement quantity by 1.
//   - EquippedConsumable:  one of the four FEquipmentLoadout::CombatConsumables
//                          slots on the target's loadout — clear slot to null.
//
// Out-of-combat uses always come from Inventory.
// In-combat uses (future "Use Item" skill) always come from EquippedConsumable.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EItemUseSource : uint8
{
    Inventory               UMETA(DisplayName = "Inventory Stack"),
    EquippedConsumable      UMETA(DisplayName = "Equipped Consumable Slot"),
};

// ---------------------------------------------------------------------------
// EItemUseResult
//
// Outcome of a TryUseItem / CanUseItem call. The Inventory and (future) Combat
// UIs use these codes to drive contextual feedback (toasts, sounds, logs).
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EItemUseResult : uint8
{
    Success,
    FailedNullItem,             // ItemDef was null
    FailedNotConsumable,        // ItemDef.Category != Consumable
    FailedNoEffects,            // ItemDef.ConsumableEffects is empty
    FailedInvalidTarget,        // Target party member not found
    FailedItemNotFound,         // Item not in inventory stack / equipped slot
    FailedTargetAlreadyAlive,   // Revive-only item used on alive target
    FailedTargetDead,           // Non-revive item used on dead target
    FailedAlreadyAtFullHP,      // Heal-only item used on full-HP target
    FailedNoApplicableEffects,  // Catch-all: no effect would do anything in this context
};

// ---------------------------------------------------------------------------
// FItemUseRequest
//
// Self-contained description of a single item use, passed to TryUseItem.
// Designed to be filled in by either the inventory UI (out of combat) or the
// future "Use Item" combat skill (in combat) — same struct, same call site.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FItemUseRequest
{
    GENERATED_BODY()

    // Item being consumed.
    UPROPERTY(BlueprintReadWrite, Category = "ItemUsage")
    TObjectPtr<UItemDefinition> ItemDef = nullptr;

    // GUID of the party member receiving the effects (FPartyMemberData::CharacterID).
    UPROPERTY(BlueprintReadWrite, Category = "ItemUsage")
    FGuid TargetCharacterID;

    // Optional caster — populated only for in-combat uses (the unit invoking the
    // Use Item skill). Used by ApplyStatus effects that need a caster snapshot
    // for percent-of-caster-stat tick formulas. Null for inventory uses.
    UPROPERTY(BlueprintReadWrite, Category = "ItemUsage")
    TObjectPtr<ACharacterBase> Caster = nullptr;

    // Where the item is consumed from (see EItemUseSource).
    UPROPERTY(BlueprintReadWrite, Category = "ItemUsage")
    EItemUseSource Source = EItemUseSource::Inventory;

    // For EquippedConsumable source: which 0..3 slot on the target's loadout
    // holds the item being consumed. Ignored for Inventory source.
    UPROPERTY(BlueprintReadWrite, Category = "ItemUsage")
    int32 EquippedSlotIndex = INDEX_NONE;
};

// ---------------------------------------------------------------------------
// UItemUsageSubsystem
//
// Single agnostic authority for consuming UItemDefinition assets.
//
// Sits between any caller (inventory UI, combat Use Item skill, scripted events)
// and the per-effect handlers. Detects whether a battle session is active via
// UGameContextManager and dispatches to the correct path:
//
//   Out of combat:  mutates FPartyMemberData on the active save (CurrentHP /
//                   bIsAlive). Status / AP effects are not applicable and items
//                   requiring them are rejected up front.
//
//   In combat:      future path — routes through CombatEventBus,
//                   StatusEffectManager, and BattleUnitState. Currently stubbed;
//                   the public API is already shaped for it.
//
// Validation:
//   CanUseItem() runs every check WITHOUT mutating state. Callers can use it
//   to decide whether to enable a Use button before opening a context menu, or
//   to show contextual feedback when blocking a wasteful use.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UItemUsageSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------

    /**
     * Returns Success if the item could be used right now with the given
     * request, or a failure code describing why it cannot.
     * Pure: does not mutate any state.
     */
    UFUNCTION(BlueprintCallable, Category = "ItemUsage")
    EItemUseResult CanUseItem(const FItemUseRequest& Request) const;

    /**
     * Validates the request, applies all applicable effects, and consumes one
     * unit of the item from the appropriate source.
     *
     * On any non-Success result, no state is mutated and no item is consumed.
     *
     * @param Request   Fully populated FItemUseRequest.
     * @return          Success or a failure code (see EItemUseResult).
     */
    UFUNCTION(BlueprintCallable, Category = "ItemUsage")
    EItemUseResult TryUseItem(const FItemUseRequest& Request);

    // -----------------------------------------------------------------------
    // Delegates
    // -----------------------------------------------------------------------

    // Broadcasts after a successful TryUseItem so widgets / VFX can react.
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnItemUsed, const FItemUseRequest& /*Request*/);
    FOnItemUsed OnItemUsed;

private:

    // -----------------------------------------------------------------------
    // Context detection
    // -----------------------------------------------------------------------

    // True when an active combat session exists (UGameContextManager state == Battle).
    bool IsInCombat() const;

    // -----------------------------------------------------------------------
    // Validation helpers
    // -----------------------------------------------------------------------

    // Verifies the item exists at the source described by Request.
    EItemUseResult ValidateSourceHasItem(const FItemUseRequest& Request) const;

    // Returns true if Effect would actually do something on the target in the
    // given context (in/out of combat, alive/dead). Used both to decide whether
    // to block the entire use and to skip individual no-op effects mid-apply.
    bool IsEffectApplicable(const FConsumableEffect& Effect, const FPartyMemberData& Target, bool bInCombat) const;

    // -----------------------------------------------------------------------
    // Effect application — out of combat
    // -----------------------------------------------------------------------

    // Returns true if any state was actually changed.
    bool ApplyEffect_OutOfCombat(const FConsumableEffect& Effect, FPartyMemberData& Target);

    // -----------------------------------------------------------------------
    // Effect application — in combat (stub for now)
    // -----------------------------------------------------------------------

    // Returns true if any state was actually changed.
    // Currently a stub: logs a TODO. Will be implemented when the Use Item
    // combat skill is added.
    bool ApplyEffect_InCombat(const FConsumableEffect& Effect, ACharacterBase* TargetActor, ACharacterBase* Caster);

    // -----------------------------------------------------------------------
    // Source consumption
    // -----------------------------------------------------------------------

    // Decrements one unit from the inventory stack of ItemDef.
    void ConsumeFromInventory(UItemDefinition* ItemDef);

    // Clears the equipped consumable slot on Target's loadout.
    void ConsumeFromEquippedSlot(FPartyMemberData& Target, int32 SlotIndex);

    // -----------------------------------------------------------------------
    // Lookup
    // -----------------------------------------------------------------------

    // Returns the PartyManagerSubsystem on the active GameInstance, or nullptr.
    // All party-member mutations route through that subsystem so the runtime
    // cache stays the single source of truth (avoids the dual-storage bug
    // where direct writes to UMyGameInstance::PartyData go invisible to UI
    // until the next SyncFromGameInstance).
    UPartyManagerSubsystem* GetPartyManager() const;

    // Logs the relevant persistent state of Target (HP, MaxHP, alive flag)
    // before/after an apply, prefixed by Phase ("PRE-USE" / "POST-USE").
    // Helps diagnose mutations that fail to land on the right cache.
    void LogTargetState(const FPartyMemberData& Target, const TCHAR* Phase) const;

    UMyGameInstance* GetMyGameInstance() const;
};
