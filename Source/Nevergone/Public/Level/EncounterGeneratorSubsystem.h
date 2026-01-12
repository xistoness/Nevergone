// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EncounterGeneratorSubsystem.generated.h"

class UBattlePreparationContext;
class UFloorEncounterData;
class AEnemyCharacter;

UCLASS()
class NEVERGONE_API UEncounterGeneratorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	void GenerateEncounter(
		const UFloorEncounterData* EncounterData,
		UBattlePreparationContext& OutPrepContext
	);
	
protected:
	const struct FEnemyEntry& SelectWeightedEnemy(const TArray<FEnemyEntry>& Entries);
};
