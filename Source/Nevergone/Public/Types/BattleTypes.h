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

UENUM()
enum class EBattleAbilitySelectionMode : uint8
{
	SingleConfirm,
	TargetThenConfirmApproach
};

UENUM()
enum class EBattleActionSelectionPhase : uint8
{
	None,
	SelectingTarget,
	SelectingApproachTile
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
	Enemy,
	None
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
	AActor* LockedTargetActor = nullptr;

	UPROPERTY()
	FIntPoint LockedTargetGridCoord;

	UPROPERTY()
	FIntPoint SelectedApproachGridCoord;

	UPROPERTY()
	bool bHasLockedTarget = false;

	UPROPERTY()
	bool bHasSelectedApproachTile = false;

	UPROPERTY()
	EBattleActionSelectionPhase SelectionPhase = EBattleActionSelectionPhase::None;

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

USTRUCT(BlueprintType)
struct NEVERGONE_API FBattleSessionData
{
	GENERATED_BODY()

public:

	// Whether this session is currently valid
	UPROPERTY()
	bool bSessionActive = false;

	// The encounter volume that triggered the combat
	UPROPERTY()
	TWeakObjectPtr<class AFloorEncounterVolume> EncounterSource;

	// Exploration character class
	UPROPERTY()
	TSubclassOf<ACharacterBase> ExplorationCharacterClass;
	
	UPROPERTY()
	ACharacterBase* ExplorationCharacter;

	// Transform of the exploration character before combat
	UPROPERTY()
	FTransform ExplorationCharacterTransform;

	// Final winner team
	UPROPERTY()
	EBattleUnitTeam WinningTeam = EBattleUnitTeam::None;

	// Number of surviving allies
	UPROPERTY()
	int32 SurvivingAllies = 0;

	// Number of surviving enemies
	UPROPERTY()
	int32 SurvivingEnemies = 0;

	// Whether combat has finished
	UPROPERTY()
	bool bCombatFinished = false;
	
	UPROPERTY()
	FRotator ExplorationControlRotation = FRotator::ZeroRotator;

	UPROPERTY()
	float ExplorationArmLength = 400.f;

	void Reset()
	{
		bSessionActive = false;
		EncounterSource = nullptr;
		ExplorationCharacterClass = nullptr;
		ExplorationCharacterTransform = FTransform::Identity;
		WinningTeam = EBattleUnitTeam::None;
		SurvivingAllies = 0;
		SurvivingEnemies = 0;
		bCombatFinished = false;
		ExplorationControlRotation = FRotator::ZeroRotator;
		ExplorationArmLength = 400.f;
	}
};