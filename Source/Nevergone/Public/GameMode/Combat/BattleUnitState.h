// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/BattleTypes.h"
#include "BattleUnitState.generated.h"

class ACharacterBase;

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
    float MaxHP = 0.f;

    UPROPERTY()
    float CurrentHP = 0.f;

    UPROPERTY()
    float PhysicalAttack = 0.f;

    UPROPERTY()
    float RangedAttack = 0.f;

    UPROPERTY()
    float MagicalPower = 0.f;

    UPROPERTY()
    float PhysicalDefense = 0.f;

    UPROPERTY()
    float MagicalDefense = 0.f;

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

    // --- Status & modifiers ---

    UPROPERTY()
    FGameplayTagContainer StatusTags;

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
        return !bIsDead && CurrentHP > 0.f;
    }

    bool CanAct() const
    {
        return IsAlive()
            && !bHasActedThisTurn
            && CurrentActionPoints > 0
            && !StatusTags.HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Status.Incapacitated")));
    }
};