// Copyright Xyzto Works

#include "GameMode/Combat/CombatFormulas.h"

#include "GameMode/Combat/BattleUnitState.h"

int32 FCombatFormulas::ComputeDamage(
    const FBattleUnitState& Source,
    const FBattleUnitState& Target,
    EAbilityDamageSource    DamageSource,
    float                   Multiplier
)
{
    // Multiplier stays float to support values like 1.5x crit.
    // Final result is rounded to int32 so damage is always a whole number.
    switch (DamageSource)
    {
    case EAbilityDamageSource::Physical:
        return FMath::Max(0, FMath::RoundToInt((Source.PhysicalAttack * Multiplier) - Target.PhysicalDefense));

    case EAbilityDamageSource::Ranged:
        return FMath::Max(0, FMath::RoundToInt((Source.RangedAttack * Multiplier) - Target.PhysicalDefense));

    case EAbilityDamageSource::Magical:
        return FMath::Max(0, FMath::RoundToInt((Source.MagicalPower * Multiplier) - Target.MagicalDefense));

    case EAbilityDamageSource::Pure:
        // Pure damage ignores all stats — Multiplier is the flat damage value
        return FMath::Max(0, FMath::RoundToInt(Multiplier));
    }

    return 0;
}

int32 FCombatFormulas::ComputeHeal(
    const FBattleUnitState& Source,
    float                   Multiplier
)
{
    return FMath::Max(0, FMath::RoundToInt(Source.MagicalPower * Multiplier));
}

float FCombatFormulas::ComputeHitChance(
    const FBattleUnitState& Source,
    const FBattleUnitState& Target,
    float                   BaseHitChance,
    bool                    bCanMiss
)
{
    if (!bCanMiss)
    {
        return 1.0f;
    }

    // Convert integer modifiers (e.g. Focus=12, Evasiveness=8) to hit chance delta
    const float ModifierDelta = (Source.HitChanceModifier - Target.EvasionModifier) * HitModifierScale;
    return FMath::Clamp(BaseHitChance + ModifierDelta, MinHitChance, MaxHitChance);
}

bool FCombatFormulas::RollHit(float HitChance)
{
    return FMath::FRand() <= HitChance;
}

float FCombatFormulas::GetCritChanceFraction(const FBattleUnitState& Source)
{
    // CritChance is stored as integer percentage (e.g. 20 = 20%)
    return FMath::Clamp(Source.CritChance * HitModifierScale, 0.f, MaxHitChance);
}

bool FCombatFormulas::RollCrit(const FBattleUnitState& Source)
{
    return FMath::FRand() <= GetCritChanceFraction(Source);
}

int32 FCombatFormulas::ApplyCritMultiplier(int32 BaseDamage)
{
    // Round crit result so 3 * 1.5 = 5 (not 4.5)
    return FMath::RoundToInt(BaseDamage * CritMultiplier);
}

int32 FCombatFormulas::EstimateDamage(
    const FBattleUnitState& Source,
    const FBattleUnitState& Target,
    EAbilityDamageSource    DamageSource,
    float                   Multiplier,
    float                   BaseHitChance,
    bool                    bCanMiss
)
{
    const int32 BaseDamage   = ComputeDamage(Source, Target, DamageSource, Multiplier);
    const float HitChance    = ComputeHitChance(Source, Target, BaseHitChance, bCanMiss);
    const float CritFraction = GetCritChanceFraction(Source);

    // Expected value rounded to int: BaseDamage * P(hit) * (1 + P(crit) * (CritMult-1))
    return FMath::RoundToInt(BaseDamage * HitChance * (1.f + CritFraction * (CritMultiplier - 1.f)));
}