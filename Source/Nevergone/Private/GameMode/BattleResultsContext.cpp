// Copyright Xyzto Works


#include "GameMode/BattleResultsContext.h"

#include "Level/FloorEncounterVolume.h"

void UBattleResultsContext::Initialize(const FBattleSessionData& InSession)
{
	SessionSnapshot = InSession;
}

void UBattleResultsContext::DumpToLog() const
{
	UE_LOG(LogTemp, Warning, TEXT("===== BATTLE RESULTS ====="));

	UE_LOG(LogTemp, Warning, TEXT("Winning Team: %d"), (int32)SessionSnapshot.WinningTeam);
	UE_LOG(LogTemp, Warning, TEXT("Surviving Allies: %d"), SessionSnapshot.SurvivingAllies);
	UE_LOG(LogTemp, Warning, TEXT("Surviving Enemies: %d"), SessionSnapshot.SurvivingEnemies);

	if (SessionSnapshot.EncounterSource.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Encounter resolved: %s"),
			*SessionSnapshot.EncounterSource->GetName());
	}
}
