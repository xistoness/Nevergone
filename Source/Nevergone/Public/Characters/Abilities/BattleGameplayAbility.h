// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Data/AbilityDefinition.h"
#include "Types/BattleTypes.h"
#include "Types/BattleInputContext.h"
#include "BattleGameplayAbility.generated.h"

class UAbilityDefinition;
class UAbilityPreviewRenderer;
class UTurnManager;
class FCombatFormulas;
struct FBattleUnitState;

UCLASS(Abstract)
class NEVERGONE_API UBattleGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UBattleGameplayAbility();

    virtual FActionResult BuildPreview(const FActionContext& Context) const;
    virtual FActionResult BuildExecution(const FActionContext& Context) const;
    virtual EBattleAbilitySelectionMode GetSelectionMode() const;
    virtual void FinalizeAbilityExecution();
    virtual void ApplyAbilityCompletionEffects();

    void CleanupAbilityDelegates();

    virtual TSubclassOf<UAbilityPreviewRenderer> GetPreviewClass() const;

    // The data asset that drives all configurable parameters for this ability.
    // Must be assigned in the Blueprint CDO.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<UAbilityDefinition> AbilityDefinition;

    bool bActivateAbilityOnGranted;

    UPROPERTY(EditDefaultsOnly, Category = "Preview")
    TSubclassOf<UAbilityPreviewRenderer> PreviewRendererClass;

protected:
    // Reads ActionPointsCost from AbilityDefinition. Returns 1 as safe fallback.
    int32 GetActionPointsCost() const;

    // Reads MaxRange from AbilityDefinition. Returns 1 as safe fallback.
    int32 GetMaxRange() const;

    // Reads TargetFaction from AbilityDefinition. Returns Enemy as safe fallback.
    EAbilityTargetFaction GetTargetFaction() const;
    
    /**
     * Plays the CastSound from AbilityDefinition at the source character's location.
     * Call this at the moment the ability fires (before damage/heal resolves).
     *
     * @param SourceCharacter  The character executing the ability
     */
    void PlayCastSFX(ACharacterBase* SourceCharacter) const;
    
    /**
     * Plays the ImpactSound from AbilityDefinition at the target's location.
     * Falls back to AudioSubsystem's DefaultHitSFX if ImpactSound is null.
     * Call this right after damage is confirmed (after ResolveAttackDamage).
     *
     * @param TargetCharacter  The character that was hit
     */
    void PlayImpactSFX(ACharacterBase* TargetCharacter) const;
    
    /**
     * Resolves the full damage from SourceState to TargetState using FCombatFormulas.
     * Rolls hit chance and crit — use for actual ability execution, not AI preview.
     * Returns 0 if the attack misses.
     */
    float ResolveAttackDamage(
        const FBattleUnitState& SourceState,
        const FBattleUnitState& TargetState
    ) const;
    

    // Resolves heal amount for SourceState using FCombatFormulas.
    float ResolveHeal(const FBattleUnitState& SourceState) const;

    // Cooldown, status effects helpers (unchanged)
    void TryStartCooldown(ACharacterBase* SourceCharacter);
    void ApplyOnHitStatusEffects(ACharacterBase* SourceCharacter, const TArray<ACharacterBase*>& HitActors);
    void ApplySelfStatusEffects(ACharacterBase* SourceCharacter);
    
    bool HasSufficientAP(const FActionContext& Context) const;

private:
    void OnTurnStateChanged(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase);
    void ClearCooldown();

    // Applies a single FAbilityStatusEffect to a target through the event bus
    void ApplyStatusEffect(
        ACharacterBase* SourceCharacter,
        ACharacterBase* TargetCharacter,
        const struct FAbilityStatusEffect& Effect
    );

    // Kept separate from any CachedCharacter in subclasses — persists after EndAbility
    UPROPERTY()
    TObjectPtr<ACharacterBase> CooldownOwner = nullptr;

    int32 RemainingCooldownTurns = 0;
    FDelegateHandle CooldownTurnHandle;
};