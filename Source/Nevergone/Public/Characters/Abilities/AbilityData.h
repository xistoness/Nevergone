// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AbilityData.generated.h"

UENUM()
enum class EActionType : uint8
{
	Melee,
	Ranged,
	Explosion,
	Knockback
};

UCLASS()
class NEVERGONE_API UAbilityData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EActionType ActionType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MaxRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float BaseDamage = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float BaseHitChance = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float AreaRadius = 0.0f;
	
};
