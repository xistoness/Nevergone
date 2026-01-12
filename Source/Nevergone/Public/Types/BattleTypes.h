// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "BattleTypes.generated.h"

USTRUCT(BlueprintType)
struct NEVERGONE_API FGeneratedPlayerData
{
	GENERATED_BODY()

	TSubclassOf<class ACharacterBase> PlayerClass;
	FGameplayTagContainer Tags;
	int32 Level = 1;
};


USTRUCT(BlueprintType)
struct NEVERGONE_API FGeneratedEnemyData
{
	GENERATED_BODY()

	TSubclassOf<class ACharacterBase> EnemyClass;
	FGameplayTagContainer Tags;
	int32 Level = 1;
};

USTRUCT(BlueprintType)
struct NEVERGONE_API FPlannedSpawn
{
	GENERATED_BODY()

	TSubclassOf<AActor> ActorClass;
	FTransform PlannedTransform;
};

class NEVERGONE_API BattleTypes
{
};

USTRUCT(BlueprintType)
struct FEnemyEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSubclassOf<class ACharacterBase> EnemyClass;

	UPROPERTY(EditAnywhere)
	float Weight = 1.0f;
	
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer Tags;
};

USTRUCT(BlueprintType)
struct FAllyEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSubclassOf<class ACharacterBase> PlayerClass;

	UPROPERTY(EditAnywhere)
	float Weight = 1.0f;
	
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer Tags;
};