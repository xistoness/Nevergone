// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "ExcursionTypes.generated.h"

class UItemDefinition;

// Why the excursion ended.
UENUM(BlueprintType)
enum class EExcursionEndReason : uint8
{
	// Left through the building exit.
	Exited,

	// The player's survivor was killed in a capture.
	SurvivorKilled,
};

// How the player's survivor comes out of the excursion.
UENUM(BlueprintType)
enum class EExcursionSurvivorCondition : uint8
{
	Unharmed,

	// Bitten at least once. The effect is applied in P2; P0 only reports it.
	Infected,

	Dead,
};

// One item type and how many units of it.
USTRUCT(BlueprintType)
struct FExcursionItemCount
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Excursion")
	TObjectPtr<UItemDefinition> Item = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Excursion")
	int32 Quantity = 0;
};

// ---------------------------------------------------------------------------
// FExcursionResult
//
// Everything the excursion produced, built once by UExcursionSubsystem when it
// ends. P0 only shows it; P2 applies it (party, infection, night buffs).
//
// Items are the difference between the shared inventory at entry and at the
// end, so every source counts (floor pickups, containers) without hooking
// each one. The price: an item found and spent in the same excursion cancels
// out, which is why safeguards used are counted on their own.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FExcursionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Excursion")
	EExcursionEndReason EndReason = EExcursionEndReason::Exited;

	UPROPERTY(BlueprintReadOnly, Category = "Excursion")
	EExcursionSurvivorCondition Condition = EExcursionSurvivorCondition::Unharmed;

	UPROPERTY(BlueprintReadOnly, Category = "Excursion")
	int32 SafeguardsUsed = 0;

	// Names of the survivors following the player through the exit.
	UPROPERTY(BlueprintReadOnly, Category = "Excursion")
	TArray<FText> RescuedSurvivors;

	// Gained during the excursion. Kept by the group on exit; lost when the
	// survivor dies.
	UPROPERTY(BlueprintReadOnly, Category = "Excursion")
	TArray<FExcursionItemCount> ItemsFound;

	// Items the group had at entry and no longer has.
	UPROPERTY(BlueprintReadOnly, Category = "Excursion")
	TArray<FExcursionItemCount> ItemsSpent;
};
