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

USTRUCT(BlueprintType)
struct FGridTraversalParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxStepUpHeight = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxStepDownHeight = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCanMoveDiagonally = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIgnoreOccupiedTiles = false;
};

UENUM()
enum class EBattleUnitTeam : uint8
{
	Ally,
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
	EBattleUnitTeam Team = EBattleUnitTeam::Ally;
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
	AActor* TargetActor = nullptr; // attack target
	
	UPROPERTY()
	bool bIsActionValid = false;
	
	UPROPERTY()
	bool bIsTileBlocked = false;

	UPROPERTY()
	FIntPoint SourceGridCoord;

	UPROPERTY()
	FIntPoint HoveredGridCoord; // tile under cursor

	UPROPERTY()
	FIntPoint MovementTargetGridCoord; // resolved adjacent tile for approach

	UPROPERTY()
	FVector SourceWorldPosition;

	UPROPERTY()
	FVector HoveredWorldPosition;

	UPROPERTY()
	FVector MovementTargetWorldPosition;

	UPROPERTY()
	FGridTraversalParams TraversalParams;

	UPROPERTY()
	int32 CachedPathCost = INDEX_NONE;

	UPROPERTY()
	TSubclassOf<UBattleGameplayAbility> AbilityClass;
	
	UPROPERTY()
	UAbilityData* Ability;
	
	UPROPERTY()
	int32 ActionPointsCost = 0;
	
	UPROPERTY()
	bool bHasLineOfSight = false;
};

USTRUCT()
struct FActionResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsValid = false;

	UPROPERTY()
	bool bRequiresMovement = false;

	UPROPERTY()
	FIntPoint MovementTargetGridCoord;

	UPROPERTY()
	FVector MovementTargetWorldPosition;

	UPROPERTY()
	TArray<FVector> PathPoints;

	UPROPERTY()
	AActor* ResolvedAttackTarget = nullptr;

	UPROPERTY()
	int32 ActionPointsCost = 0;

	UPROPERTY()
	float ExpectedDamage = 0.f;
	
	UPROPERTY()
	float HitChance = 0.f;
	
	UPROPERTY()
	TArray<AActor*> AffectedActors;
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