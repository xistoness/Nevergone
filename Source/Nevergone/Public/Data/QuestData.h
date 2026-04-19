// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "QuestData.generated.h"

// ---------------------------------------------------------------------------
// EQuestState
// ---------------------------------------------------------------------------

/** Runtime lifecycle state of a quest. */
UENUM(BlueprintType)
enum class EQuestState : uint8
{
    NotStarted  UMETA(DisplayName = "Not Started"),
    Active      UMETA(DisplayName = "Active"),
    Completed   UMETA(DisplayName = "Completed"),
    Failed      UMETA(DisplayName = "Failed"),
};

// ---------------------------------------------------------------------------
// FQuestStep
//
// One entry in the quest diary. Steps form a directed graph: completing a step
// unlocks zero or more other steps. The set of initially-available steps is
// defined on UQuestData::InitialUnlockedStepIndices.
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FQuestStep
{
    GENERATED_BODY()

    // Unique index of this step within the quest. Must match the array index
    // in UQuestData::Steps — used as the stable ID in runtime state and save data.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest Step")
    int32 StepIndex = 0;

    // Text shown in the quest diary for this step (e.g. "Talk to Aldric about the key").
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest Step", meta = (MultiLine = true))
    FText DiaryEntry;

    // Steps that become available once this step is completed.
    // Leave empty if this step is a terminal node.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest Step")
    TArray<int32> UnlocksStepIndices;

    // If true, completing this step also sets the quest state to Completed.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest Step")
    bool bFinishesQuest = false;

    // If true, completing this step sets the quest state to Failed.
    // Takes precedence over bFinishesQuest if both are set.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest Step")
    bool bFailsQuest = false;
};

// ---------------------------------------------------------------------------
// FQuestRuntimeState
//
// Persisted per-quest runtime data. Stored in MySaveGame::QuestRuntimeStates.
// Replaces the previous TMap<FName, EQuestState> — state is now a field here.
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FQuestRuntimeState
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    EQuestState State = EQuestState::NotStarted;

    // Indices of steps the player has already completed (crossed out in diary).
    UPROPERTY(SaveGame)
    TArray<int32> CompletedStepIndices;

    // Indices of steps currently available to be completed.
    // Populated from UQuestData::InitialUnlockedStepIndices on StartQuest,
    // and updated by AdvanceQuestStep as the graph is traversed.
    UPROPERTY(SaveGame)
    TArray<int32> UnlockedStepIndices;
};

// ---------------------------------------------------------------------------
// UQuestData
// ---------------------------------------------------------------------------

/**
 * UQuestData
 *
 * Static definition of a quest stored as a DataAsset.
 * Runtime state lives in UQuestSubsystem keyed by the asset's primary FName.
 *
 * Steps form a directed graph. Author the graph by:
 *  1. Adding entries to Steps (StepIndex must match array position).
 *  2. Setting InitialUnlockedStepIndices to the steps available at quest start.
 *  3. Filling UnlocksStepIndices on each step to express branching.
 */
UCLASS(BlueprintType)
class NEVERGONE_API UQuestData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
    FText Title;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = true))
    FText Description;

    // All steps in this quest. StepIndex on each entry must equal its position
    // in this array (enforced by convention, not at runtime).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
    TArray<FQuestStep> Steps;

    // Which step indices are available immediately when the quest starts.
    // For a strictly linear quest this is just {0}.
    // For a quest that opens with multiple parallel objectives, list them all.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
    TArray<int32> InitialUnlockedStepIndices;

    // Returns the step with the given index, or nullptr if not found.
    const FQuestStep* FindStep(int32 StepIndex) const
    {
        return Steps.FindByPredicate([StepIndex](const FQuestStep& S)
        {
            return S.StepIndex == StepIndex;
        });
    }
};
