// Copyright Xyzto Works

#include "World/QuestSubsystem.h"
#include "GameInstance/MyGameInstance.h"
#include "GameInstance/MySaveGame.h"

void UQuestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        GI->OnSaveLoaded.AddUObject(this, &UQuestSubsystem::SyncFromGameInstance);
        SyncFromGameInstance();

        UE_LOG(LogTemp, Log, TEXT("[QuestSubsystem] Initialized."));
    }
}

// ---------------------------------------------------------------------------
// Quest lifecycle
// ---------------------------------------------------------------------------

void UQuestSubsystem::StartQuest(FName QuestName)
{
    const EQuestState Current = GetQuestState(QuestName);
    if (Current != EQuestState::NotStarted)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[QuestSubsystem] StartQuest: '%s' is already in state %d — ignoring."),
            *QuestName.ToString(), (int32)Current);
        return;
    }

    FQuestRuntimeState& Runtime = QuestRuntimeStates.FindOrAdd(QuestName);
    Runtime.State = EQuestState::Active;
    Runtime.CompletedStepIndices.Reset();
    Runtime.UnlockedStepIndices.Reset();

    // Populate initial unlocked steps from the data asset.
    if (UQuestData* Asset = LoadQuestAsset(QuestName))
    {
        Runtime.UnlockedStepIndices = Asset->InitialUnlockedStepIndices;

        UE_LOG(LogTemp, Log,
            TEXT("[QuestSubsystem] StartQuest: '%s' started with %d initial step(s) unlocked."),
            *QuestName.ToString(), Runtime.UnlockedStepIndices.Num());
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[QuestSubsystem] StartQuest: could not load asset for '%s' — no steps unlocked."),
            *QuestName.ToString());
    }

    OnQuestStateChanged.Broadcast(QuestName, EQuestState::Active);
}

void UQuestSubsystem::CompleteQuest(FName QuestName)
{
    FQuestRuntimeState* Runtime = QuestRuntimeStates.Find(QuestName);
    if (!Runtime || Runtime->State != EQuestState::Active)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[QuestSubsystem] CompleteQuest: '%s' is not Active — ignoring."),
            *QuestName.ToString());
        return;
    }

    Runtime->State = EQuestState::Completed;

    UE_LOG(LogTemp, Log, TEXT("[QuestSubsystem] Quest completed: %s"), *QuestName.ToString());
    OnQuestStateChanged.Broadcast(QuestName, EQuestState::Completed);
}

void UQuestSubsystem::FailQuest(FName QuestName)
{
    FQuestRuntimeState* Runtime = QuestRuntimeStates.Find(QuestName);
    if (!Runtime || Runtime->State != EQuestState::Active)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[QuestSubsystem] FailQuest: '%s' is not Active — ignoring."),
            *QuestName.ToString());
        return;
    }

    Runtime->State = EQuestState::Failed;

    UE_LOG(LogTemp, Log, TEXT("[QuestSubsystem] Quest failed: %s"), *QuestName.ToString());
    OnQuestStateChanged.Broadcast(QuestName, EQuestState::Failed);
}

// ---------------------------------------------------------------------------
// Step advancement
// ---------------------------------------------------------------------------

void UQuestSubsystem::AdvanceQuestStep(FName QuestName, int32 StepIndex)
{
    FQuestRuntimeState* Runtime = QuestRuntimeStates.Find(QuestName);
    if (!Runtime)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[QuestSubsystem] AdvanceQuestStep: quest '%s' not found in runtime state."),
            *QuestName.ToString());
        return;
    }

    if (Runtime->State != EQuestState::Active)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[QuestSubsystem] AdvanceQuestStep: quest '%s' is not Active (state %d) — ignoring."),
            *QuestName.ToString(), (int32)Runtime->State);
        return;
    }

    // Only steps that are currently unlocked can be advanced.
    if (!Runtime->UnlockedStepIndices.Contains(StepIndex))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[QuestSubsystem] AdvanceQuestStep: step %d is not unlocked for quest '%s' — ignoring."),
            StepIndex, *QuestName.ToString());
        return;
    }

    // Move step from unlocked to completed.
    Runtime->UnlockedStepIndices.Remove(StepIndex);
    Runtime->CompletedStepIndices.AddUnique(StepIndex);

    UE_LOG(LogTemp, Log,
        TEXT("[QuestSubsystem] Quest '%s' step %d completed."),
        *QuestName.ToString(), StepIndex);

    OnQuestStepAdvanced.Broadcast(QuestName, StepIndex);

    // Load the asset to read which steps this completion unlocks, and whether
    // it ends or fails the quest.
    UQuestData* Asset = LoadQuestAsset(QuestName);
    if (!Asset)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[QuestSubsystem] AdvanceQuestStep: could not load asset for '%s' — skipping step graph traversal."),
            *QuestName.ToString());
        return;
    }

    const FQuestStep* Step = Asset->FindStep(StepIndex);
    if (!Step)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[QuestSubsystem] AdvanceQuestStep: step %d not found in asset '%s'."),
            StepIndex, *GetNameSafe(Asset));
        return;
    }

    // Unlock the steps this step points to.
    for (int32 NextStep : Step->UnlocksStepIndices)
    {
        // Skip steps already completed or already unlocked.
        if (!Runtime->CompletedStepIndices.Contains(NextStep) &&
            !Runtime->UnlockedStepIndices.Contains(NextStep))
        {
            Runtime->UnlockedStepIndices.Add(NextStep);

            UE_LOG(LogTemp, Log,
                TEXT("[QuestSubsystem] Quest '%s' step %d unlocked by step %d."),
                *QuestName.ToString(), NextStep, StepIndex);
        }
    }

    // bFailsQuest takes precedence over bFinishesQuest.
    if (Step->bFailsQuest)
    {
        FailQuest(QuestName);
    }
    else if (Step->bFinishesQuest)
    {
        CompleteQuest(QuestName);
    }
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

EQuestState UQuestSubsystem::GetQuestState(FName QuestName) const
{
    const FQuestRuntimeState* Runtime = QuestRuntimeStates.Find(QuestName);
    return Runtime ? Runtime->State : EQuestState::NotStarted;
}

bool UQuestSubsystem::IsQuestActive(FName QuestName) const
{
    return GetQuestState(QuestName) == EQuestState::Active;
}

bool UQuestSubsystem::IsQuestCompleted(FName QuestName) const
{
    return GetQuestState(QuestName) == EQuestState::Completed;
}

bool UQuestSubsystem::IsStepCompleted(FName QuestName, int32 StepIndex) const
{
    const FQuestRuntimeState* Runtime = QuestRuntimeStates.Find(QuestName);
    return Runtime && Runtime->CompletedStepIndices.Contains(StepIndex);
}

bool UQuestSubsystem::IsStepUnlocked(FName QuestName, int32 StepIndex) const
{
    const FQuestRuntimeState* Runtime = QuestRuntimeStates.Find(QuestName);
    return Runtime && Runtime->UnlockedStepIndices.Contains(StepIndex);
}

bool UQuestSubsystem::GetRuntimeState(FName QuestName, FQuestRuntimeState& OutState) const
{
    const FQuestRuntimeState* Runtime = QuestRuntimeStates.Find(QuestName);
    if (!Runtime) { return false; }
    OutState = *Runtime;
    return true;
}

TArray<FName> UQuestSubsystem::GetQuestsInState(EQuestState State) const
{
    TArray<FName> Result;
    for (const TPair<FName, FQuestRuntimeState>& Pair : QuestRuntimeStates)
    {
        if (Pair.Value.State == State)
        {
            Result.Add(Pair.Key);
        }
    }
    return Result;
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

void UQuestSubsystem::SyncFromGameInstance()
{
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) { return; }

    QuestRuntimeStates = GI->QuestRuntimeStates;

    UE_LOG(LogTemp, Log,
        TEXT("[QuestSubsystem] SyncFromGameInstance: loaded %d quest runtime states."),
        QuestRuntimeStates.Num());
}

void UQuestSubsystem::FlushToGameInstance()
{
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) { return; }

    GI->SetQuestRuntimeStates(QuestRuntimeStates);

    UE_LOG(LogTemp, Log,
        TEXT("[QuestSubsystem] FlushToGameInstance: flushed %d quest runtime states."),
        QuestRuntimeStates.Num());
}

void UQuestSubsystem::Reset()
{
    QuestRuntimeStates.Empty();
    FlushToGameInstance();

    UE_LOG(LogTemp, Log, TEXT("[QuestSubsystem] Reset: all quest runtime states cleared."));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

UQuestData* UQuestSubsystem::LoadQuestAsset(FName QuestName) const
{
    const FString AssetPath = FString::Printf(TEXT("/Game/Data/QuestData/%s.%s"),
        *QuestName.ToString(), *QuestName.ToString());

    UQuestData* Asset = LoadObject<UQuestData>(nullptr, *AssetPath);
    if (!Asset)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[QuestSubsystem] LoadQuestAsset: could not find '%s'."), *AssetPath);
    }
    return Asset;
}
