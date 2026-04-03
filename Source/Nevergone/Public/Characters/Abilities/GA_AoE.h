// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "TargetingRules/CompositeTargetingPolicy.h"
#include "GA_AoE.generated.h"

class ACharacterBase;
class UBattleState;

/**
 * Generic area-of-effect ability.
 *
 * Defines HOW the ability is executed: target a tile within range,
 * gather all valid actors within AoERadius, apply an effect to each.
 *
 * WHAT the effect is — damage, heal, status, faction, radius, range —
 * is entirely driven by the assigned AbilityDefinition. This class has
 * no hardcoded values.
 *
 * Example configurations via AbilityDefinition:
 *   EffectType=Heal,   TargetFaction=Ally,  AoERadius=1 → "Heal AoE"
 *   EffectType=Damage, TargetFaction=Enemy, AoERadius=2 → "Explosion"
 *   EffectType=None,   OnHitStatusEffects=[Poison]      → "Acid Cloud"
 */
UCLASS()
class NEVERGONE_API UGA_AoE : public UBattleGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_AoE();

    virtual FActionResult BuildPreview(const FActionContext& Context) const override;
    virtual FActionResult BuildExecution(const FActionContext& Context) const override;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;

    virtual void ApplyAbilityCompletionEffects() override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled
    ) override;

private:
    bool BuildAoEResult(const FActionContext& Context, FActionResult& OutResult) const;

    // Gathers all characters within AoERadius of CenterCoord that match
    // the faction filter from AbilityDefinition->TargetFaction.
    // Uses BattleUnitState for alive checks — not UnitStatsComponent.
    bool GatherAffectedActors(
        ACharacterBase* SourceCharacter,
        UBattleState* BattleStateRef,
        const FIntPoint& CenterCoord,
        TArray<ACharacterBase*>& OutActors
    ) const;

    // Dispatches the effect configured in AbilityDefinition->EffectType
    // to each actor in CachedAffectedActors.
    void ApplyResolvedEffect();

    void FinalizeAction();

private:
    UPROPERTY()
    TObjectPtr<ACharacterBase> CachedCharacter = nullptr;

    UPROPERTY()
    TArray<TObjectPtr<ACharacterBase>> CachedAffectedActors;

    int32 CachedActionPointsCost = 0;
    FIntPoint CachedCenterCoord = FIntPoint::ZeroValue;
};