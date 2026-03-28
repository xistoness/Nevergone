// Copyright Xyzto Works


#include "Characters/EnemyUnitBase.h"

#include "GameMode/Combat/AI/BattleAIComponent.h"

AEnemyUnitBase::AEnemyUnitBase()
{
	BattleAIComponent = CreateDefaultSubobject<UBattleAIComponent>(TEXT("BattleAIComponent"));
}
