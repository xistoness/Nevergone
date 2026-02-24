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

	/* ----- Identity ----- */

	UPROPERTY()
	TWeakObjectPtr<ACharacterBase> UnitActor;

	UPROPERTY()
	EBattleUnitTeam Team = EBattleUnitTeam::Player;

	/* ----- Core stats ----- */

	UPROPERTY()
	float MaxHP = 100.0f;

	UPROPERTY()
	float CurrentHP = 100.0f;
	
	UPROPERTY()
	float MaxSpeed = 7.0f;

	UPROPERTY()
	float CurrentSpeed = 7.0f;
	
	UPROPERTY()
	float MaxMeleeAttack = 10.0f;
	
	UPROPERTY()
	float MaxRangedAttack = 10.0f;
	
	UPROPERTY()
	int32 ActionPoints = 0;

	/* ----- Status & modifiers ----- */

	UPROPERTY()
	FGameplayTagContainer StatusTags;

	/* ----- State flags ----- */
	
	UPROPERTY()
	bool bIsDead = false;
	
	UPROPERTY()
	bool bHasMovedThisTurn = false;
	
	UPROPERTY()
	bool bHasActedThisTurn = false;

	/* ----- Queries ----- */

	bool IsAlive() const
	{
		return !bIsDead && CurrentHP > 0.0f;
	}

	bool CanAct() const
	{
		// Tags like Status.Stun, Status.Sleep, etc.
		return IsAlive() && !bHasActedThisTurn && !StatusTags.HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Status.Incapacitated")));
	}
};