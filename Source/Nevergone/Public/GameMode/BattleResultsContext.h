// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/BattleTypes.h"
#include "BattleResultsContext.generated.h"

/**
 * Immutable snapshot of everything that happened in a battle session.
 * Created by GameContextManager when entering BattleResults state,
 * passed to the controller and widget for display.
 *
 * Designed to grow: add XP gained, loot, persistent status changes, etc.
 * here without touching GameContextManager or the widget directly.
 */
UCLASS()
class NEVERGONE_API UBattleResultsContext : public UObject
{
	GENERATED_BODY()

public:

	// Snapshot of the session data at combat end.
	UPROPERTY(BlueprintReadOnly, Category = "Results")
	FBattleSessionData SessionSnapshot;

	// Convenience accessors for the widget — avoids exposing FBattleSessionData to BP directly.
	UFUNCTION(BlueprintPure, Category = "Results")
	bool IsVictory() const;

	UFUNCTION(BlueprintPure, Category = "Results")
	int32 GetSurvivingAllies() const { return SessionSnapshot.SurvivingAllies; }

	UFUNCTION(BlueprintPure, Category = "Results")
	int32 GetSurvivingEnemies() const { return SessionSnapshot.SurvivingEnemies; }

	void Initialize(const FBattleSessionData& InSession);
	void DumpToLog() const;
};