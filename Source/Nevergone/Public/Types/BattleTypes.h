// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "LevelTypes.h"
#include "BattleTypes.generated.h"

class UAbilityData;
class UBattleGameplayAbility;

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

UENUM()
enum class EBattleUnitTeam : uint8
{
	Player,
	Enemy
};

USTRUCT()
struct FSpawnedBattleUnit
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<class ACharacterBase> UnitActor;
	
	UPROPERTY()
	int32 SourceIndex = INDEX_NONE;

	UPROPERTY()
	EBattleUnitTeam Team = EBattleUnitTeam::Player;
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
	int32 Level = 1;
	
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
	int32 Level = 1;
	
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer Tags;
};

USTRUCT()
struct FActionContext
{
	GENERATED_BODY()

	UPROPERTY()
	AActor* SourceActor = nullptr;

	UPROPERTY()
	AActor* TargetActor = nullptr;
	
	UPROPERTY()
	FIntPoint SourceGridCoord;

	UPROPERTY()
	FIntPoint TargetGridCoord;

	UPROPERTY()
	FVector SourceWorldPosition;
	
	UPROPERTY()
	FVector TargetWorldPosition;
	
	UPROPERTY()
	int32 CachedPathCost = INDEX_NONE;
	
	UPROPERTY()
	bool bHasLineOfSight = false;
	
	UPROPERTY()
	bool bIsTileBlocked = false;
	
	UPROPERTY()
	bool bIsActionValid = false;
	
	UPROPERTY()
	bool bIsPreview = false;
	
	UPROPERTY()
	int32 ActionPointsCost = 0;

	UPROPERTY()
	UAbilityData* Ability = nullptr;
	
	UPROPERTY()
	TSubclassOf<UBattleGameplayAbility> AbilityClass;

};

USTRUCT()
struct FActionResult
{
	GENERATED_BODY()

	UPROPERTY()
	float HitChance = 0.0f;

	UPROPERTY()
	float ExpectedDamage = 0.0f;

	UPROPERTY()
	TArray<AActor*> AffectedActors;

	UPROPERTY()
	bool bIsValid = false;
	
	FIntPoint TargetGridCoord;
	
	int32 ActionPointsCost = 0;
	
	FVector TargetWorldPosition = FVector::ZeroVector;
};

// Target data representing a single grid tile
USTRUCT()
struct FGameplayAbilityTargetData_Tile : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	// Tile coordinates in grid space
	UPROPERTY()
	FIntPoint TileCoord;

	// Optional actor on that tile
	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}
};