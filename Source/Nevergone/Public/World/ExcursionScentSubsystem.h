// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ExcursionScentSubsystem.generated.h"

class AExcursionScentBait;

// ---------------------------------------------------------------------------
// UExcursionScentSubsystem
//
// The registry of scent sources an enemy can smell besides a survivor: today
// the thrown scent baits (AExcursionScentBait), which register themselves on
// BeginPlay and leave on EndPlay, as the noise mask volumes do with
// UExcursionNoiseSubsystem. The smell poll of UExplorationPerceptionComponent
// asks here instead of iterating actors.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UExcursionScentSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	static UExcursionScentSubsystem* Get(const UObject* WorldContextObject);

	void RegisterScentBait(AExcursionScentBait* Bait);

	void UnregisterScentBait(AExcursionScentBait* Bait);

	// The baits whose scent radius reaches Location, closest first.
	void GatherScentBaitsAt(const FVector& Location, TArray<AExcursionScentBait*>& OutBaits) const;

protected:

	// Game and PIE only, like the noise subsystem.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:

	TArray<TWeakObjectPtr<AExcursionScentBait>> ScentBaits;
};
