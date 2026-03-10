// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "CharacterTypes.generated.h"

USTRUCT(BlueprintType)
struct FUnitAbilityEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName ActionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UBattleGameplayAbility> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAbilityData* AbilityData = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bGrantedAtBattleStart = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bIsDefaultAction = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bIsMovementAbility = false;
};