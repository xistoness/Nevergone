// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/BattleTypes.h"
#include "Engine/DataAsset.h"
#include "FloorEncounterData.generated.h"

UCLASS()
class NEVERGONE_API UFloorEncounterData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere)
	TArray<FEnemyEntry> PossibleEnemies;

	UPROPERTY(EditAnywhere)
	TArray<FAllyEntry> PossibleAllies;
	
	UPROPERTY(EditAnywhere)
	int32 MinTotalEnemies = 1;

	UPROPERTY(EditAnywhere)
	int32 MaxTotalEnemies = 5;
};
