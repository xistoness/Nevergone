// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "GA_SelfCast.generated.h"

class ACharacterBase;

/**
 * Self-targeting ability execution class.
 *
 * Resolves immediately on the caster — no grid targeting, no range check.
 * Selection mode is SingleConfirm: the ability fires as soon as the player
 * confirms it from the hotbar, with no cursor targeting phase.
 *
 * Activation gate: requires CurrentActionPoints >= 1 (not a specific cost).
 * AP drain is handled entirely by a DrainActionPoints entry in
 * AbilityDefinition->SelfStatusEffects, so the ability can always fire
 * regardless of how many AP the unit has left.
 *
 * Other effects are driven by AbilityDefinition:
 *   - SelfStatusEffects : status effects applied to the caster (including DrainActionPoints)
 *   - CooldownTurns     : optional cooldown after use
 *
 * Examples via AbilityDefinition:
 *   SelfStatusEffects=[DrainActionPoints]          → "Skip Turn" (exhaust all AP, no other effect)
 *   SelfStatusEffects=[Regen, DrainActionPoints]   → "Meditate" (buff self, then exhaust AP)
 *   SelfStatusEffects=[Barrier], CooldownT=2       → "Defensive Stance" (costs 1 AP via normal cost field)
 */
UCLASS()
class NEVERGONE_API UGA_SelfCast : public UBattleGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_SelfCast();

    // Returns SingleConfirm — no targeting phase required for self-cast abilities.
    virtual EBattleAbilitySelectionMode GetSelectionMode() const override;

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
    // Validates AP and populates OutResult. Shared between BuildPreview and BuildExecution.
    bool BuildSelfCastResult(const FActionContext& Context, FActionResult& OutResult) const;

private:
    UPROPERTY()
    TObjectPtr<ACharacterBase> CachedCharacter = nullptr;
};