// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EncounterPartySubsystem.generated.h"

class UFloorEncounterData;
class UBattlePreparationContext;

UCLASS()
class NEVERGONE_API UEncounterPartySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void GeneratePlayerParty(
	const UFloorEncounterData* EncounterData,
	UBattlePreparationContext& OutPrepContext
);
};
