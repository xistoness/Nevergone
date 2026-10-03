// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ExcursionEncounterSettings.generated.h"

// ---------------------------------------------------------------------------
// UExcursionEncounterSettings
//
// Constants of the capture death-chance formula, which is the same for every
// enemy type. Per-type values (base chance, knockdown and devour times) live
// on UExplorationEnemyDefinition; the formula itself lives in
// UExcursionEncounterSubsystem.
//
// Edited in Project Settings > Game > Excursion Encounter and stored in
// DefaultGame.ini, so tuning needs no recompile.
// ---------------------------------------------------------------------------
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Excursion Encounter"))
class NEVERGONE_API UExcursionEncounterSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	// Percentage points the death chance drops per point of Strength.
	UPROPERTY(Config, EditAnywhere, Category = "Capture", meta = (ClampMin = "0.0"))
	float DeathChanceReductionPerStrength = 2.0f;

	// Percentage points the death chance rises per earlier bite taken by the
	// same survivor in the same excursion.
	UPROPERTY(Config, EditAnywhere, Category = "Capture", meta = (ClampMin = "0.0"))
	float DeathChanceBonusPerPriorBite = 25.0f;
};
