// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/BattleTypes.h"
#include "UObject/Object.h"
#include "BattlePreparationContext.generated.h"

class UGridManager;
class AFloorEncounterVolume;

UCLASS()
class NEVERGONE_API UBattlePreparationContext : public UObject
{
	GENERATED_BODY()

public:
	
	
	/* --- Enemy side --- */
	UPROPERTY(BlueprintReadWrite)
	TArray<FGeneratedEnemyData> EnemyParty;
	
	UPROPERTY(BlueprintReadWrite)
	TArray<FPlannedSpawn> EnemyPlannedSpawns;
	
	/* --- Player side --- */
	UPROPERTY(BlueprintReadWrite)
	TArray<FGeneratedPlayerData> PlayerParty;
	UPROPERTY(BlueprintReadWrite)
	TArray<FPlannedSpawn> PlayerPlannedSpawns;
	
	/* --- Spawned runtime units --- */
	UPROPERTY()
	TArray<FSpawnedBattleUnit> SpawnedUnits;
	
	/* --- Metadata --- */
	UPROPERTY(BlueprintReadOnly)
	int32 FloorIndex;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTagContainer CombatTags;

	// The encounter volume that triggered this battle.
	// Passed through to CombatManager so it can be recorded in mid-combat saves.
	UPROPERTY()
	TObjectPtr<AFloorEncounterVolume> EncounterSource;
	
	/* --- Validation --- */
	void Initialize();
	bool IsReadyToStartCombat() const;
	
	void Reset();
	void DumpToLog() const;
};