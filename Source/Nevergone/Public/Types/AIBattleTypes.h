// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "AIBattleTypes.generated.h"

class ACharacterBase;
class UBattleGameplayAbility;
class UBattleAIQueryService;
class UGridManager;
class UUnitStatsComponent;

UENUM(BlueprintType)
enum class ETeamAIActionType : uint8
{
	None,
	UseAbility,
	Move,
	EndTurn
};

USTRUCT(BlueprintType)
struct FAITargetingData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ACharacterBase> TargetActor = nullptr;

	UPROPERTY()
	FIntPoint TargetTile = FIntPoint::ZeroValue;

	UPROPERTY()
	bool bHasActorTarget = false;

	UPROPERTY()
	bool bHasTileTarget = false;
};

USTRUCT()
struct FAIScoreContribution
{
	GENERATED_BODY()

	UPROPERTY()
	FString RuleName;

	UPROPERTY()
	float Value = 0.f;
};

USTRUCT()
struct FTeamTurnContext
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<ACharacterBase>> AlliedUnits;

	UPROPERTY()
	TArray<TObjectPtr<ACharacterBase>> EnemyUnits;
	
	UPROPERTY()
	TArray<TObjectPtr<ACharacterBase>> AllCombatants;

	UPROPERTY()
	TSet<TObjectPtr<ACharacterBase>> UnitsThatAlreadyActed;

	UPROPERTY()
	TMap<TObjectPtr<ACharacterBase>, float> ReservedDamageByTarget;

	UPROPERTY()
	TSet<FIntPoint> ReservedTiles;

	UPROPERTY()
	TObjectPtr<ACharacterBase> FocusTarget = nullptr;

	UPROPERTY()
	bool bTurnShouldEnd = false;
};

USTRUCT(BlueprintType)
struct FAIBehaviorProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Aggression = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float KillFocus = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AllySupport = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SelfPreservation = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ObjectiveFocus = 1.f;
};

USTRUCT(BlueprintType)
struct FTeamCandidateAction
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ACharacterBase> ActingUnit = nullptr;

	UPROPERTY()
	ETeamAIActionType ActionType = ETeamAIActionType::None;

	UPROPERTY()
	TSubclassOf<UBattleGameplayAbility> AbilityClass = nullptr;

	UPROPERTY()
	FAITargetingData Targeting;

	UPROPERTY()
	float Score = 0.f;

	UPROPERTY()
	FString Reason;
	
	UPROPERTY()
	TArray<FAIScoreContribution> ScoreBreakdown;

	UPROPERTY()
	int32 ExpectedAPCost = 0;

	UPROPERTY()
	float ExpectedDamage = 0.f;

	UPROPERTY()
	bool bConsumesMainAction = true;
};

USTRUCT()
struct FAICandidateEvalContext
{
	GENERATED_BODY()

	ACharacterBase* ActingUnit = nullptr;
	const FTeamTurnContext* TurnContext = nullptr;
	UBattleAIQueryService* QueryService = nullptr;
	UGridManager* Grid = nullptr;
	UUnitStatsComponent* ActingStats = nullptr;
	TArray<ACharacterBase*> Allies;
	TArray<ACharacterBase*> Enemies;
};



