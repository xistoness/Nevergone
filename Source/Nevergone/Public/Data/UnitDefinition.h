// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/BattleTypes.h"
#include "Types/CharacterTypes.h"

#include "Engine/DataAsset.h"
#include "UnitDefinition.generated.h"


UCLASS()
class NEVERGONE_API UUnitDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 MaxLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float BaseHP;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float HPPerLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float BaseSpeed;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float BaseMeleeAttack;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float BaseRangedAttack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float BaseActionPoints;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AActor> UnitClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGridTraversalParams TraversalParams;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Battle")
	TArray<FUnitAbilityEntry> BattleAbilities;
};
