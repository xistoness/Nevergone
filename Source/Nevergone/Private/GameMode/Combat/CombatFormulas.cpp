// Copyright Xyzto Works

#include "GameMode/Combat/CombatFormulas.h"

#include "GameMode/Combat/BattleUnitState.h"

float FCombatFormulas::ComputeDamage(
    const FBattleUnitState& Source,
    const FBattleUnitState& Target,
    EAbilityDamageSource    DamageSource,
    float                   Multiplier
)
{
    switch (DamageSource)
    {
    case EAbilityDamageSource::Physical:
        return FMath::Max(0.f, (Source.PhysicalAttack * Multiplier) - Target.PhysicalDefense);

    case EAbilityDamageSource::Ranged:
        return FMath::Max(0.f, (Source.RangedAttack * Multiplier) - Target.PhysicalDefense);

    case EAbilityDamageSource::Magical:
        return FMath::Max(0.f, (Source.MagicalPower * Multiplier) - Target.MagicalDefense);

    case EAbilityDamageSource::Pure:
        // Pure damage ignores all stats — Multiplier is the flat damage value
        return FMath::Max(0.f, Multiplier);
    }

    return 0.f;
}

float FCombatFormulas::ComputeHeal(
    const FBattleUnitState& Source,
    float                   Multiplier
)
{
    return FMath::Max(0.f, Source.MagicalPower * Multiplier);
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

float FCombatFormulas::ApplyCritMultiplier(float BaseDamage)
{
    return BaseDamage * CritMultiplier;
}

float FCombatFormulas::EstimateDamage(
    const FBattleUnitState& Source,
    const FBattleUnitState& Target,
    EAbilityDamageSource    DamageSource,
    float                   Multiplier,
    float                   BaseHitChance,
    bool                    bCanMiss
)
{
    const float BaseDamage  = ComputeDamage(Source, Target, DamageSource, Multiplier);
    const float HitChance   = ComputeHitChance(Source, Target, BaseHitChance, bCanMiss);
    const float CritFraction = GetCritChanceFraction(Source);

    // Expected value: damage * P(hit) * (1 + P(crit) * (CritMult - 1))
    // Simplified: BaseDamage * HitChance * (1 + CritFraction * 0.5)
    return BaseDamage * HitChance * (1.f + CritFraction * (CritMultiplier - 1.f));
}