// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/BattleTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattlefieldLayoutSubsystem.generated.h"

class UBattlePreparationContext;
class AFloorEncounterVolume;
class UBattlefieldQuerySubsystem;


UCLASS()
class NEVERGONE_API UBattlefieldLayoutSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void PlanSpawns(UBattlefieldQuerySubsystem& BattlefieldQuery, UBattlePreparationContext& BattlePrepContext);

	
private:
	void PlanEnemySpawns(UBattlefieldQuerySubsystem& BattlefieldQuery, UBattlePreparationContext& BattlePrepContext);
	void PlanAllySpawns(UBattlefieldQuerySubsystem& BattlefieldQuery, UBattlePreparationContext& BattlePrepContext);
};
