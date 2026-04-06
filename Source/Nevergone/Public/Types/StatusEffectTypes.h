// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/BattleTypes.h"
#include "StatusEffectTypes.generated.h"

class UStatusEffectDefinition;
class ACharacterBase;

// ---------------------------------------------------------------------------
// FActiveStatusEffect
//
// Runtime instance of a single status effect active on a unit.
// Stored in FBattleUnitState::ActiveStatusEffects.
//
// One UStatusEffectDefinition can produce multiple FActiveStatusEffect entries
// when StackBehavior == Stack (e.g. two independent Poison stacks from
// different casters each have their own instance with their own duration
// and cached caster stat).
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct NEVERGONE_API FActiveStatusEffect
{
    GENERATED_BODY()

    // The asset that defines all parameters for this effect.
    UPROPERTY()
    TObjectPtr<UStatusEffectDefinition> Definition = nullptr;

    // The unit that applied this effect. Needed for PercentOfCasterStat tick formula
    // and StartOfCasterTurn tick timing. Weak pointer: caster may die before expiry.
    UPROPERTY()
    TWeakObjectPtr<ACharacterBase> Caster;

    // Team the caster belonged to when this effect was applied.
    // Used by StatusEffectManager to identify "caster's turn" even if the caster
    // actor is no longer alive.
    UPROPERTY()
    EBattleUnitTeam CasterTeam = EBattleUnitTeam::Ally;

    // Turns remaining before this instance expires.
    // Decremented once per player-turn cycle (same cadence as ability cooldowns).
    // 0 = permanent — never decremented, never expires on its own.
    UPROPERTY()
    int32 TurnsRemaining = 1;

    // Current HP of the shield layer. Only meaningful for Shield passive effects.
    // 0 on all non-Shield instances.
    UPROPERTY()
    int32 ShieldHP = 0;

    // Snapshot of the relevant caster stat at the time of application.
    // Populated by the ability that applies this effect when TickFormula == PercentOfCasterStat
    // or PassiveEffect == Shield, so calculations survive caster death.
    // Which stat to snapshot is decided by the ability (matching its own DamageSource),
    // not by this Definition — see UStatusEffectManager::ApplyStatusEffect overload.
    UPROPERTY()
    int32 CachedCasterStatValue = 0;

    // Unique ID within a single unit's ActiveStatusEffects list.
    // Used to identify a specific instance during stacked removal or expiry.
    UPROPERTY()
    int32 InstanceId = INDEX_NONE;

    bool IsValid() const { return Definition != nullptr; }
};