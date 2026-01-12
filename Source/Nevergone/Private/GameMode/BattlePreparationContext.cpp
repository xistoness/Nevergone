// Copyright Xyzto Works


#include "GameMode/BattlePreparationContext.h"


bool UBattlePreparationContext::IsReadyToStartCombat() const
{
	return EnemyParty.Num() > 0 &&
	EnemyPlannedSpawns.Num() == EnemyParty.Num() &&
	PlayerParty.Num() > 0 &&
	PlayerPlannedSpawns.Num() == PlayerParty.Num() &&
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
