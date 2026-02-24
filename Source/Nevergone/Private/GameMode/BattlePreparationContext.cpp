// Copyright Xyzto Works


#include "GameMode/BattlePreparationContext.h"
#include "Level/GridManager.h"


void UBattlePreparationContext::Initialize()
{

}

bool UBattlePreparationContext::IsReadyToStartCombat() const
{
	UE_LOG(LogTemp, Warning, TEXT("Checking if battle preparation is ready to start combat..."));
	DumpToLog();
	return EnemyParty.Num() > 0 &&
	EnemyPlannedSpawns.Num() == EnemyParty.Num() &&
	PlayerParty.Num() > 0 &&
	PlayerPlannedSpawns.Num() > 0;
}

void UBattlePreparationContext::Reset()
{
	EnemyParty.Reset();
	EnemyPlannedSpawns.Reset();
	PlayerParty.Reset();
	PlayerPlannedSpawns.Reset();
	CombatTags.Reset();
	FloorIndex = INDEX_NONE;
}

void UBattlePreparationContext::DumpToLog() const
{
	UE_LOG(LogTemp, Warning, TEXT("=== BattlePreparationContext ==="));
	UE_LOG(LogTemp, Warning, TEXT("Enemies: %d"), EnemyParty.Num());
	UE_LOG(LogTemp, Warning, TEXT("Players: %d"), PlayerParty.Num());
	UE_LOG(LogTemp, Warning, TEXT("Enemy Spawns: %d"), EnemyPlannedSpawns.Num());
	UE_LOG(LogTemp, Warning, TEXT("Player Spawns: %d"), PlayerPlannedSpawns.Num());
}
