// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "CharacterTypes.generated.h"

class UAbilityDefinition;

USTRUCT(BlueprintType)
struct FUnitAbilityEntry
{
	GENERATED_BODY()

	// The definition that fully describes this ability — class, stats, effects, cost.
	// AbilityClass is read from Definition->AbilityClass at grant time.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAbilityDefinition> Definition = nullptr;

	// Whether this ability is granted to the unit at the start of battle.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bGrantedAtBattleStart = true;

	// Whether this is the default selected ability when the unit's turn begins.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bIsDefaultAction = false;

	// Whether this is the unit's movement ability.
	// Movement abilities are handled differently from combat abilities in
	// BattleModeComponent and the AI system.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bIsMovementAbility = false;
};