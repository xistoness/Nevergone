// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "ActorComponents/MyAbilitySystemComponent.h"
#include "Characters/CharacterBase.h"
#include "Types/BattleTypes.h"
#include "Types/StatusEffectTypes.h"
#include "BattleUnitState.generated.h"

USTRUCT()
struct FBattleUnitState
{
    GENERATED_BODY()

    // --- Identity ---

    UPROPERTY()
    TWeakObjectPtr<ACharacterBase> UnitActor;

    UPROPERTY()
    EBattleUnitTeam Team = EBattleUnitTeam::Ally;

    // --- Concrete combat stats ---

    UPROPERTY()
    int32 MaxHP = 0;

    UPROPERTY()
    int32 CurrentHP = 0;

    UPROPERTY()
    int32 PhysicalAttack = 0;

    UPROPERTY()
    int32 RangedAttack = 0;

    UPROPERTY()
    int32 MagicalPower = 0;

    UPROPERTY()
    int32 PhysicalDefense = 0;

    UPROPERTY()
    int32 MagicalDefense = 0;

    UPROPERTY()
    int32 MaxActionPoints = 0;

    UPROPERTY()
    int32 CurrentActionPoints = 0;

    UPROPERTY()
    int32 MovementRange = 0;

    UPROPERTY()
    int32 HitChanceModifier = 0;

    UPROPERTY()
    int32 EvasionModifier = 0;

    UPROPERTY()
    int32 CritChance = 0;

    UPROPERTY()
    FGridTraversalParams TraversalParams;

    // --- Status effects ---

    // All currently active status effect instances on this unit.
    // Managed exclusively by UStatusEffectManager — do not modify directly.
    UPROPERTY()
    TArray<FActiveStatusEffect> ActiveStatusEffects;

    // --- Turn state flags ---

    UPROPERTY()
    bool bIsDead = false;

    UPROPERTY()
    bool bHasMovedThisTurn = false;

    UPROPERTY()
    bool bHasActedThisTurn = false;

    // --- Queries ---

    bool IsAlive() const
    {
        return !bIsDead && CurrentHP > 0;
    }

    bool CanAct() const
    {
        if (!IsAlive() || bHasActedThisTurn || CurrentActionPoints <= 0)
        {
            return false;
        }

        // Status tags that block action are applied to the unit's ASC by StatusEffectManager.
        // Reading from the ASC here keeps a single source of truth: any blocking status
        // (present or future) that sets the right tag will be respected automatically.
        ACharacterBase* Unit = UnitActor.Get();
        if (!Unit) { return false; }

        const UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
        if (!ASC) { return true; }

        // State.Stunned      — unit cannot act at all this turn
        // Status.Incapacitated — permanent incapacitation (future use)
        // State.Charmed      — unit is under enemy control; blocked for its original team's turn
        //                      (TurnManager skips it; the enemy AI picks it up on enemy turn)
        static const FGameplayTag StunnedTag       = FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"));
        static const FGameplayTag CharmedTag       = FGameplayTag::RequestGameplayTag(TEXT("State.Charmed"));

        return !ASC->HasMatchingGameplayTag(StunnedTag)
            && !ASC->HasMatchingGameplayTag(CharmedTag);
    }
};