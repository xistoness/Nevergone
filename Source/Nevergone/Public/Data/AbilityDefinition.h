// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "AbilityDefinition.generated.h"

class UBattleGameplayAbility;

// Determines which source stat drives the damage calculation and which
// defensive stat on the target mitigates it.
//
//   Physical : Source.PhysicalAttack  vs Target.PhysicalDefense
//   Ranged   : Source.RangedAttack    vs Target.PhysicalDefense
//   Magical  : Source.MagicalPower    vs Target.MagicalDefense
//   Pure     : flat DamageMultiplier, ignores all stats on both sides
UENUM(BlueprintType)
enum class EAbilityDamageSource : uint8
{
    Physical,
    Ranged,
    Magical,
    Pure,
};

// What kind of effect this ability resolves on each affected actor.
UENUM(BlueprintType)
enum class EAbilityEffectType : uint8
{
    None,
    Damage,
    Heal,
};

// Which team the ability targets when resolving hits.
UENUM(BlueprintType)
enum class EAbilityTargetFaction : uint8
{
    Enemy,
    Ally,
    Any,
};

USTRUCT(BlueprintType)
struct FAbilityStatusEffect
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FGameplayTag StatusTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FString DisplayLabel;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<UTexture2D> Icon = nullptr;

    // How many full turn cycles the status lasts (0 = permanent until cleared manually)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
    int32 DurationTurns = 1;
};

UCLASS()
class NEVERGONE_API UAbilityDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    // --- UI ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<UTexture2D> Icon;

    // --- Classification ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FGameplayTag AbilityTag;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TSubclassOf<UBattleGameplayAbility> AbilityClass;

    // --- Targeting ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    EAbilityTargetFaction TargetFaction = EAbilityTargetFaction::Enemy;

    // --- Range ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
    int32 MinRange = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
    int32 MaxRange = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bRequiresLineOfSight = false;

    // Radius of the area-of-effect around the targeted tile (0 = single tile).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
    int32 AoERadius = 0;

    // --- Cost ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
    int32 ActionPointsCost = 1;

    // --- Effect ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    EAbilityEffectType EffectType = EAbilityEffectType::Damage;

    // Which stat drives the calculation. Ignored when EffectType == Heal.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    EAbilityDamageSource DamageSource = EAbilityDamageSource::Physical;

    // Multiplied against the relevant source stat to produce raw output.
    //   Damage: FinalDamage = (SourceAttack * DamageMultiplier) - TargetDefense
    //   Heal  : FinalHeal   = SourceMagicalPower * HealMultiplier
    //   Pure  : FinalDamage = DamageMultiplier (flat, ignores stats)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
    float DamageMultiplier = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
    float HealMultiplier = 1.0f;

    // Base hit chance [0..1]. Abilities with bCanMiss=false always hit.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BaseHitChance = 1.0f;

    // If false, hit chance is always 1.0 — Focus/Evasion stats are ignored.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bCanMiss = false;

    // --- Cooldown ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
    int32 CooldownTurns = 0;

    // --- Status Effects ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<FAbilityStatusEffect> OnHitStatusEffects;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<FAbilityStatusEffect> SelfStatusEffects;
    
    // --- Audio ---
 
    /**
     * Sound played at the SOURCE actor's location when this ability executes.
     * Examples: sword swing, staff charge, bow draw.
     * Played before damage/heal is applied — represents the action itself.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    TObjectPtr<USoundBase> CastSound;
 
    /**
     * Sound played at the TARGET actor's location when this ability connects.
     * Examples: sword impact, arrow hit, explosion.
     * Played at the moment of hit — distinct from the cast sound.
     * Leave null to use the AudioSubsystem's DefaultHitSFX fallback.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    TObjectPtr<USoundBase> ImpactSound;
 
    /**
     * Sound played at the SOURCE actor's location when this ability is
     * used on a movement (footstep-style or dash whoosh).
     * Only relevant for movement abilities — leave null for attacks/heals.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    TObjectPtr<USoundBase> MovementSound;
};