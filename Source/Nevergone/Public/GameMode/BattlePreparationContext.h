// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/BattleTypes.h"
#include "UObject/Object.h"
#include "BattlePreparationContext.generated.h"

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
	
	/* --- Metadata --- */
	UPROPERTY(BlueprintReadOnly)
	int32 FloorIndex;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTagContainer CombatTags;
	
	/* --- Validation --- */
	bool IsReadyToStartCombat() const;
	
	void Reset();
	
};
