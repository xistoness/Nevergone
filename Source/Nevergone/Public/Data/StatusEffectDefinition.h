// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StatusEffectDefinition.generated.h"

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

// Broad classification used for UI categorization (buff vs debuff).
UENUM(BlueprintType)
enum class EStatusEffectCategory : uint8
{
    Debuff,
    Buff,
};

// Controls what happens when the same status is applied to a unit that already has it.
UENUM(BlueprintType)
enum class EStatusStackBehavior : uint8
{
    // Second application is silently ignored (e.g. Stun).
    Ignore,

    // Duration resets to full on re-application (e.g. Slow).
    Refresh,

    // A new independent instance is added; both tick and expire separately (e.g. Poison).
    Stack,
};

// When per-turn tick effects fire relative to the turns of the victim or caster.
UENUM(BlueprintType)
enum class EStatusTickTiming : uint8
{
    // No per-turn tick (e.g. Stun, Slow, Silence — purely passive effects).
    None,

    // Fires at the start of the afflicted unit's own turn (e.g. Bleed).
    StartOfVictimTurn,

    // Fires at the end of the afflicted unit's own turn.
    EndOfVictimTurn,

    // Fires at the start of the caster's team turn (e.g. Poison).
    // Use bTickOnApplication = true if the first tick should fire immediately on application.
    StartOfCasterTurn,

    // Fires at the end of the caster's team turn.
    EndOfCasterTurn,
};

// What the per-turn tick effect actually does.
UENUM(BlueprintType)
enum class EStatusTickEffect : uint8
{
    // No per-turn effect.
    None,

    // Deals damage each tick. Amount determined by EStatusTickFormula.
    DamagePerTurn,

    // Restores HP each tick. Amount determined by EStatusTickFormula.
    HealPerTurn,
};

// How the per-turn damage or heal amount is calculated.
UENUM(BlueprintType)
enum class EStatusTickFormula : uint8
{
    // Fixed value defined in TickFlatAmount (e.g. Burn: always 5 damage).
    Flat,

    // Percentage of the victim's MaxHP (e.g. Bleed: 5% MaxHP).
    PercentOfTargetMaxHP,

    // Percentage of a caster stat snapshotted at application time.
    // Which stat is chosen by the ability that applies this effect, not by this Definition —
    // it is stored in FActiveStatusEffect::CachedCasterStatValue at application time.
    PercentOfCasterStat,
};

// What passive (non-tick) modification this status applies to the unit.
UENUM(BlueprintType)
enum class EStatusPassiveEffect : uint8
{
    // No stat modification.
    None,

    // Reduces MovementRange by a percentage.
    // Formula: penalty = Floor(CurrentMovementRange * PassiveEffectTargetMultiplier)
    ReduceMovementRangePercent,

    // Increases MovementRange by a percentage.
    // Formula: bonus = Floor(CurrentMovementRange * PassiveEffectTargetMultiplier)
    IncreaseMovementRangePercent,

    // Grants a damage-absorbing shield.
    // Formula: ShieldHP = PassiveEffectFlatAmount + Floor(CasterStat * PassiveEffectCasterMultiplier)
    // CasterStat is determined by the ability applying this effect (see ResolveCasterStatSnapshot).
    Shield,

    // Grants bonus AP immediately on application (e.g. Respite: +2 AP).
    // Formula: AP += PassiveEffectFlatAmount
    GrantActionPoints,

    // Removes listed status tags on application, then expires immediately (e.g. Cure).
    CleansesStatuses,

    // Unit acts for the opposing team for the duration (e.g. Charm).
    SwitchTeam,
};

// ---------------------------------------------------------------------------
// UStatusEffectDefinition
// ---------------------------------------------------------------------------

UCLASS(BlueprintType)
class NEVERGONE_API UStatusEffectDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:

    // -----------------------------------------------------------------------
    // UI
    // -----------------------------------------------------------------------

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TObjectPtr<UTexture2D> Icon;

    // Short string shown in floating text when this status is applied (e.g. "STUN").
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    FString DisplayLabel;

    // Text shown in UI that explains the status effects and rules.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    FString Description;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    EStatusEffectCategory Category = EStatusEffectCategory::Debuff;

    // -----------------------------------------------------------------------
    // Identity
    // -----------------------------------------------------------------------

    // Gameplay tag applied to the unit's ASC for GAS integration.
    // This drives ActivationBlockedTags on abilities
    // (e.g. State.Stunned blocks all, State.Silenced blocks magical abilities).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FGameplayTag StatusTag;

    // -----------------------------------------------------------------------
    // Duration & Stacking
    // -----------------------------------------------------------------------

    // How many full player-turn cycles this status lasts. 0 = permanent until manually cleared.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duration", meta = (ClampMin = "0"))
    int32 DurationTurns = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duration")
    EStatusStackBehavior StackBehavior = EStatusStackBehavior::Ignore;

    // -----------------------------------------------------------------------
    // Per-turn tick
    // -----------------------------------------------------------------------

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tick")
    EStatusTickTiming TickTiming = EStatusTickTiming::None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tick")
    EStatusTickEffect TickEffect = EStatusTickEffect::None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tick")
    EStatusTickFormula TickFormula = EStatusTickFormula::Flat;

    // Used when TickFormula == Flat.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tick", meta = (ClampMin = "0.0"))
    float TickFlatAmount = 0.f;

    // Used when TickFormula == PercentOfTargetMaxHP or PercentOfCasterStat. Range [0..1].
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tick",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TickPercent = 0.f;

    // If true, one tick fires immediately when this status is applied, in addition to
    // the periodic schedule. Useful for Poison, Burn, etc. so damage happens right away
    // on the caster's turn rather than waiting a full cycle.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tick")
    bool bTickOnApplication = false;

    // -----------------------------------------------------------------------
    // Passive effect (applied immediately, reversed on expiry or removal)
    // -----------------------------------------------------------------------

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Passive")
    EStatusPassiveEffect PassiveEffect = EStatusPassiveEffect::None;

    // Flat integer value used in passive effect formulas.
    // Examples:
    //   GrantActionPoints  → AP += PassiveEffectFlatAmount
    //   Shield             → ShieldHP = PassiveEffectFlatAmount + Floor(CasterStat * PassiveEffectCasterMultiplier)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Passive", meta = (ClampMin = "0"))
    int32 PassiveEffectFlatAmount = 0;

    // Multiplier applied to a caster stat snapshotted at application time.
    // Which stat is used is determined by the ability, not this Definition —
    // it is stored in FActiveStatusEffect::CachedCasterStatValue.
    // Examples:
    //   Shield             → ShieldHP += Floor(CachedCasterStatValue * PassiveEffectCasterMultiplier)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Passive", meta = (ClampMin = "0.0"))
    float PassiveEffectCasterMultiplier = 0.f;

    // Multiplier applied to a target stat (e.g. current MovementRange).
    // Examples:
    //   ReduceMovementRangePercent  → penalty = Floor(MovementRange * PassiveEffectTargetMultiplier)
    //   IncreaseMovementRangePercent → bonus = Floor(MovementRange * PassiveEffectTargetMultiplier)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Passive",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PassiveEffectTargetMultiplier = 0.f;

    // Used by CleansesStatuses: status tags removed on application.
    // After cleansing, this instance expires immediately regardless of DurationTurns.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Passive")
    TArray<FGameplayTag> TagsToCleanse;
};