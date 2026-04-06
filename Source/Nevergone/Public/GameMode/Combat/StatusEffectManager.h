// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Types/BattleInputContext.h"
#include "Types/StatusEffectTypes.h"
#include "StatusEffectManager.generated.h"

class ACharacterBase;
class UBattleState;
class UCombatEventBus;
class UTurnManager;
class UStatusEffectDefinition;
enum class EStatusTickTiming: uint8;

// ---------------------------------------------------------------------------
// UStatusEffectManager
//
// Lives on UCombatManager for the duration of a battle session.
// Central authority for applying, ticking, and removing status effects.
//
// Design:
//   - All runtime data lives in FBattleUnitState::ActiveStatusEffects, not here.
//   - Listens to TurnManager::OnTurnStateChanged for tick and expiry timing.
//   - All mutations route through CombatEventBus and BattleState so the rest
//     of the codebase stays decoupled from status internals.
// ---------------------------------------------------------------------------

UCLASS()
class NEVERGONE_API UStatusEffectManager : public UObject
{
    GENERATED_BODY()

public:

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    // Must be called once after CombatManager creates all subsystems.
    void Initialize(UBattleState* InBattleState, UCombatEventBus* InEventBus, UTurnManager* InTurnManager);

    // Unsubscribes from TurnManager — call from CombatManager::Cleanup().
    void Shutdown();

    // -----------------------------------------------------------------------
    // Application
    // -----------------------------------------------------------------------

    /**
     * Attempts to apply a status effect from Caster to Target.
     *
     * Handles in order:
     *   1. Null / dead-unit guards
     *   2. Immunity check (ASC tag pattern "Immunity.<StatusTag>")
     *   3. Stack behavior: Ignore / Refresh / Stack
     *   4. Passive effect application (Slow, Shield, Charm, Respite, Cure)
     *   5. ASC tag grant (for GAS ActivationBlockedTags integration)
     *   6. CombatEventBus notification (floating text, delegates)
     *   7. Immediate tick if Definition->bTickOnApplication is true
     *
     * @param Caster             Unit that caused the effect. May be null (terrain source).
     * @param Target             Unit receiving the effect.
     * @param Definition         Asset that defines all parameters for this effect.
     * @param CasterStatSnapshot Snapshot of the relevant caster stat for PercentOfCasterStat
     *                           tick formula and Shield passive. Pass 0 for effects that
     *                           don't use caster stats (Flat, PercentOfTargetMaxHP, Stun, etc.).
     *                           The ability calling this knows its own DamageSource and should
     *                           resolve the right stat before calling.
     */
    void ApplyStatusEffect(
        ACharacterBase*          Caster,
        ACharacterBase*          Target,
        UStatusEffectDefinition* Definition,
        int32                    CasterStatSnapshot = 0
    );

    // -----------------------------------------------------------------------
    // Removal
    // -----------------------------------------------------------------------

    /**
     * Removes ALL active instances of StatusTag from Target.
     * Reverses passive effects (movement penalty, team swap, etc.).
     * Removes the ASC tag when the last instance is gone.
     */
    void RemoveStatusEffect(ACharacterBase* Target, const FGameplayTag& StatusTag);

    /**
     * Removes a single instance by InstanceId.
     * Used internally by the expiry system and AbsorbDamageWithShield.
     */
    void RemoveStatusEffectInstance(ACharacterBase* Target, int32 InstanceId);

    // -----------------------------------------------------------------------
    // Shield
    // -----------------------------------------------------------------------

    /**
     * Absorbs IncomingDamage through Shield instances on Target.
     * Depletes shields in order; removes instances that reach 0 HP.
     * Returns the damage that passes through to the unit's HP.
     *
     * Must be called by CombatEventBus::NotifyDamageApplied BEFORE
     * mutating BattleState HP.
     */
    int32 AbsorbDamageWithShield(ACharacterBase* Target, int32 IncomingDamage);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    // Returns true if Target has at least one active instance of this tag.
    bool HasStatusEffect(const ACharacterBase* Target, const FGameplayTag& StatusTag) const;

    // Returns total shield HP across all Shield instances on Target.
    int32 GetShieldHP(const ACharacterBase* Target) const;

private:

    // -----------------------------------------------------------------------
    // Turn tick
    // -----------------------------------------------------------------------

    void OnTurnStateChanged(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase);

    // Fires tick effects for instances whose TickTiming matches the current phase.
    void TickStatusesForTiming(EStatusTickTiming Timing, EBattleUnitTeam ActiveTeam);

    // Applies one tick of damage or heal for a single active instance.
    void TickEffectInstance(ACharacterBase* Target, FActiveStatusEffect& Instance);

    // Decrements TurnsRemaining for all non-permanent instances and removes expired ones.
    // Called once per full round (start of player turn).
    void DecrementAndExpireStatuses();

    // -----------------------------------------------------------------------
    // Passive effect helpers
    // -----------------------------------------------------------------------

    // Applies the passive portion of a status on application.
    void ApplyPassiveEffect(ACharacterBase* Target, FActiveStatusEffect& Instance);

    // Reverses the passive portion when an instance is removed or expires.
    void RevertPassiveEffect(ACharacterBase* Target, const FActiveStatusEffect& Instance);

    // Handles CleansesStatuses: removes listed tags then marks the Cure instance for immediate expiry.
    void ApplyCleanse(ACharacterBase* Target, FActiveStatusEffect& Instance);

    // -----------------------------------------------------------------------
    // Immunity
    // -----------------------------------------------------------------------

    // Returns true if Target's ASC holds the immunity tag for this definition.
    // Tag pattern: "Immunity." + StatusTag.GetTagName() with dots replaced by underscores.
    // Example: "State.Stunned" -> checks for "Immunity.State_Stunned".
    // Any source (UnitDefinition innate tags, equipment, terrain) can grant these.
    bool IsImmune(ACharacterBase* Target, const UStatusEffectDefinition* Definition) const;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    // Resolves the per-tick damage or heal amount for an instance.
    float ResolveTickAmount(const ACharacterBase* Target, const FActiveStatusEffect& Instance) const;

    // Returns the next unique instance ID.
    int32 GenerateInstanceId();

    // -----------------------------------------------------------------------
    // Injected references
    // -----------------------------------------------------------------------

    UPROPERTY()
    TObjectPtr<UBattleState> BattleState;

    UPROPERTY()
    TObjectPtr<UCombatEventBus> EventBus;

    UPROPERTY()
    TObjectPtr<UTurnManager> TurnManager;

    FDelegateHandle TurnStateHandle;

    // Monotonically increasing counter for InstanceId generation.
    int32 NextInstanceId = 0;
};