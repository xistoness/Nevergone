// Copyright Xyzto Works


#include "Level/EncounterPartySubsystem.h"
#include "GameMode/BattlePreparationContext.h"
#include "Level/FloorEncounterData.h"
#include "Tests/ToolMenusTestUtilities.h"
#include "Types/BattleTypes.h"

void UEncounterPartySubsystem::GeneratePlayerParty(const UFloorEncounterData* EncounterData,
	UBattlePreparationContext& OutPrepContext)
{
	if (!EncounterData)
		return;
	
	OutPrepContext.PlayerParty.Reset();
	
	for (int32 i = 0; i < sizeof(EncounterData->PossibleAllies); ++i)
	{
		FAllyEntry AllyEntry = EncounterData->PossibleAllies[i];
		FGeneratedPlayerData NewAlly;
		NewAlly.PlayerClass = AllyEntry.PlayerClass;
		NewAlly.Tags = AllyEntry.Tags;
		NewAlly.Level = 1; // Neutral default for now

		OutPrepContext.PlayerParty.Add(NewAlly);
	}
}
