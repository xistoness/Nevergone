// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Data/AbilityDefinition.h"

struct FBattleUnitState;

/**
 * Pure static utility class containing all combat resolution formulas.
 *
 * All methods are stateless — they take the relevant stats as parameters
 * so they can be called from abilities, AI scoring rules, or any other
 * system without coupling to a specific actor or component.
 *
 * To extend the formula system (e.g. equipment bonuses, weather effects),
 * add additional parameters to the relevant methods or add new overloads.
 */
class NEVERGONE_API FCombatFormulas
{
public:
    FCombatFormulas() = delete;

    // -----------------------------------------------------------------------
    // Damage
    // -----------------------------------------------------------------------

    /**
     * Computes final damage dealt from Source to Target.
     *
     * Physical : (PhysicalAttack  * Multiplier) - PhysicalDefense, min 0
     * Ranged   : (RangedAttack    * Multiplier) - PhysicalDefense, min 0
     * Magical  : (MagicalPower    * Multiplier) - MagicalDefense,  min 0
     * Pure     : Multiplier (flat, both stats ignored)
     */
    static int32 ComputeDamage(
        const FBattleUnitState& Source,
        const FBattleUnitState& Target,
        EAbilityDamageSource    DamageSource,
        float                   Multiplier
    );

    // -----------------------------------------------------------------------
    // Healing
    // -----------------------------------------------------------------------

    /**
     * Computes final healing applied by Source.
     * FinalHeal = MagicalPower * Multiplier
     *
     * NOTE: MagicalPower is used as the heal stat until a dedicated HealPower
     * stat is added to UnitDefinition. Update this method when that happens.
     */
    static int32 ComputeHeal(
        const FBattleUnitState& Source,
        float                   Multiplier
    );

    // -----------------------------------------------------------------------
    // Hit resolution
    // -----------------------------------------------------------------------

    /**
     * Computes the final hit chance for an attack.
     *
     * If bCanMiss is false, always returns 1.0.
     * Otherwise: clamp(BaseHitChance + (Source.HitChanceModifier - Target.EvasionModifier) / 100, 0.05, 0.95)
     *
     * HitChanceModifier and EvasionModifier are integer values (e.g. 12 means 12%).
     * Dividing by 100 converts them to the [0..1] range used for hit chance.
     */
    static float ComputeHitChance(
        const FBattleUnitState& Source,
        const FBattleUnitState& Target,
        float                   BaseHitChance,
        bool                    bCanMiss
    );

    /**
     * Rolls against HitChance and returns true if the attack connects.
     * Uses FMath::FRand() — not deterministic across clients.
     */
    static bool RollHit(float HitChance);

    // -----------------------------------------------------------------------
    // Critical hits
    // -----------------------------------------------------------------------

    /**
     * Returns the critical hit chance as a value in [0..1].
     * CritChance in BattleUnitState is stored as a percentage (e.g. 20 = 20%).
     */
    static float GetCritChanceFraction(const FBattleUnitState& Source);

    /**
     * Rolls against CritChance and returns true if the attack is a critical hit.
     */
    static bool RollCrit(const FBattleUnitState& Source);

    /**
     * Applies the critical hit multiplier to a base damage value.
     * CritMultiplier = 1.5x
     */
    static int32 ApplyCritMultiplier(int32 BaseDamage);

    // -----------------------------------------------------------------------
    // AI estimation (no RNG — returns expected values for scoring)
    // -----------------------------------------------------------------------

    /**
     * Returns the expected damage for AI scoring purposes.
     * Does not roll RNG — returns: ComputeDamage * HitChance * (1 + CritChance * 0.5)
     * The last factor accounts for the expected damage bonus from crits.
     */
    static int32 EstimateDamage(
        const FBattleUnitState& Source,
        const FBattleUnitState& Target,
        EAbilityDamageSource    DamageSource,
        float                   Multiplier,
        float                   BaseHitChance,
        bool                    bCanMiss
    );

private:
    static constexpr float CritMultiplier    = 1.5f;
    static constexpr float MinHitChance      = 0.05f;
    static constexpr float MaxHitChance      = 0.95f;
    static constexpr float HitModifierScale  = 0.01f; // converts integer modifier to [0..1]
};