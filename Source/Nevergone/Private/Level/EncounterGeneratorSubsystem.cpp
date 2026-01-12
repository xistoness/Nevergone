// Copyright Xyzto Works


#include "Characters/CharacterBase.h"
#include "Level/EncounterGeneratorSubsystem.h"
#include "Level/FloorEncounterData.h"
#include "GameMode/BattlePreparationContext.h"
#include "Types/BattleTypes.h"

void UEncounterGeneratorSubsystem::GenerateEncounter(const UFloorEncounterData* EncounterData,
	UBattlePreparationContext& OutPrepContext)
{
	if (!EncounterData)
		return;

	const int32 TargetEnemyCount = FMath::RandRange(
		EncounterData->MinTotalEnemies,
		EncounterData->MaxTotalEnemies
	);

	OutPrepContext.EnemyParty.Reset();

	for (int32 i = 0; i < TargetEnemyCount; ++i)
	{
		const FEnemyEntry& Entry =
			SelectWeightedEnemy(EncounterData->PossibleEnemies);

		FGeneratedEnemyData NewEnemy;
		NewEnemy.EnemyClass = Entry.EnemyClass;
		NewEnemy.Tags = Entry.Tags;
		NewEnemy.Level = 1; // Neutral default for now

		OutPrepContext.EnemyParty.Add(NewEnemy);
	}
}

const FEnemyEntry& UEncounterGeneratorSubsystem::SelectWeightedEnemy(
	const TArray<FEnemyEntry>& Entries
)
{
	float TotalWeight = 0.0f;
	for (const FEnemyEntry& Entry : Entries)
	{
		TotalWeight += Entry.Weight;
	}

	const float Roll = FMath::FRandRange(0.0f, TotalWeight);

	float Accumulator = 0.0f;
	for (const FEnemyEntry& Entry : Entries)
	{
		Accumulator += Entry.Weight;
		if (Roll <= Accumulator)
		{
			return Entry;
		}
	}

	// Fallback (should never happen)
	return Entries.Last();
}