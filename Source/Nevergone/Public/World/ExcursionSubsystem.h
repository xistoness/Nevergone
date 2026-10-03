// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/ExcursionTypes.h"
#include "ExcursionSubsystem.generated.h"

class ACharacter;
class AExplorationEnemyCharacter;
class UExcursionEncounterSubsystem;
class UItemDefinition;
enum class EExcursionCaptureOutcome : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnExcursionEnded, const FExcursionResult& /*Result*/);

// ---------------------------------------------------------------------------
// UExcursionSubsystem
//
// Owns one excursion from entry to result.
//
//   Start   When the building's world begins play, the shared inventory is
//           counted per item. That count is the baseline of the result.
//   End     Through the exit door (ExitExcursion), or when a capture kills
//           the player's survivor (heard on OnCaptureResolved).
//   Result  Condition of the survivor, safeguards used, rescued survivors
//           (the ones following at the exit) and the item difference against
//           the baseline. When the survivor dies, what was found is removed
//           from the inventory: loot only stays with the group on exit.
//
// The result is broadcast on OnExcursionEnded; the player controller shows
// it. The subsystem knows nothing about UI.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UExcursionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	static UExcursionSubsystem* Get(const UObject* WorldContextObject);

	// Called by AExcursionExit. Ignored once the excursion has ended.
	void ExitExcursion();

	// P0 playtest loop: puts the inventory back as it was at entry and
	// reopens the building, so every attempt starts from the same state.
	void RestartExcursion();

	bool HasEnded() const { return bEnded; }

	const FExcursionResult& GetResult() const { return Result; }

	FOnExcursionEnded OnExcursionEnded;

protected:

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual void Deinitialize() override;

	// Game and PIE only, like the other excursion subsystems.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:

	void HandleCaptureResolved(ACharacter* Victim, AExplorationEnemyCharacter* Enemy, EExcursionCaptureOutcome Outcome);

	void EndExcursion(EExcursionEndReason Reason);

	// Units per item type in the shared inventory right now.
	TMap<TObjectPtr<UItemDefinition>, int32> CountInventory() const;

	// Removes or adds each count through the inventory subsystem.
	void RemoveItems(const TArray<FExcursionItemCount>& Items) const;
	void AddItems(const TArray<FExcursionItemCount>& Items) const;

	// Baseline taken at entry.
	UPROPERTY()
	TMap<TObjectPtr<UItemDefinition>, int32> EntryInventory;

	UPROPERTY()
	FExcursionResult Result;

	TWeakObjectPtr<UExcursionEncounterSubsystem> EncounterSubsystem;

	int32 BitesTaken = 0;

	int32 SafeguardsUsed = 0;

	bool bEnded = false;
};
