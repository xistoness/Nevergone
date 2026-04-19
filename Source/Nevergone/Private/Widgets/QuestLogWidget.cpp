// Copyright Xyzto Works

#include "Widgets/QuestLogWidget.h"
#include "World/QuestSubsystem.h"

void UQuestLogWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UGameInstance* GI = GetGameInstance();
    if (!GI) { return; }

    QuestSubsystem = GI->GetSubsystem<UQuestSubsystem>();
    if (!QuestSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[QuestLogWidget] Failed to find QuestSubsystem."));
        return;
    }

    QuestSubsystem->OnQuestStateChanged.AddUObject(this, &UQuestLogWidget::HandleQuestStateChanged);
    QuestSubsystem->OnQuestStepAdvanced.AddUObject(this, &UQuestLogWidget::HandleQuestStepAdvanced);

    RefreshQuestLog();

    UE_LOG(LogTemp, Log, TEXT("[QuestLogWidget] Constructed."));
}

void UQuestLogWidget::NativeDestruct()
{
    if (QuestSubsystem)
    {
        QuestSubsystem->OnQuestStateChanged.RemoveAll(this);
        QuestSubsystem->OnQuestStepAdvanced.RemoveAll(this);
    }

    Super::NativeDestruct();
}

void UQuestLogWidget::RefreshQuestLog()
{
    if (!QuestSubsystem) { return; }

    BP_ClearQuestLog();
    EntryCount = 0;

    const TArray<EQuestState> OrderedStates = {
        EQuestState::Active,
        EQuestState::Completed,
        EQuestState::Failed
    };

    UE_LOG(LogTemp, Log, TEXT("[QuestLogWidget] Refreshing quest log..."));

    for (EQuestState State : OrderedStates)
    {
        TArray<FName> Quests = QuestSubsystem->GetQuestsInState(State);

        for (const FName& QuestName : Quests)
        {
            UQuestData* Asset = LoadQuestAsset(QuestName);

            const FText Title       = Asset ? Asset->Title       : FText::FromName(QuestName);
            const FText Description = Asset ? Asset->Description : FText::GetEmpty();

            BP_BeginQuestEntry(Title, Description, State);

            UE_LOG(LogTemp, Log,
                TEXT("[QuestLogWidget] Added quest entry [%d]: %s"), EntryCount, *Title.ToString());

            if (Asset)
            {
                FQuestRuntimeState RuntimeState;
                const bool bHasRuntime = QuestSubsystem->GetRuntimeState(QuestName, RuntimeState);

                for (const FQuestStep& Step : Asset->Steps)
                {
                    const bool bCompleted = bHasRuntime &&
                        RuntimeState.CompletedStepIndices.Contains(Step.StepIndex);
                    const bool bUnlocked  = bHasRuntime &&
                        RuntimeState.UnlockedStepIndices.Contains(Step.StepIndex);

                    if (bCompleted || bUnlocked)
                    {
                        BP_AddQuestStep(Step.DiaryEntry, bCompleted);
                    }
                }
            }

            BP_EndQuestEntry();
            ++EntryCount;
        }
    }
    
    BP_StartQuestLog();
    UE_LOG(LogTemp, Log,
        TEXT("[QuestLogWidget] RefreshQuestLog complete. %d entries."), EntryCount);
}

// ---------------------------------------------------------------------------
// Keyboard navigation
// ---------------------------------------------------------------------------

void UQuestLogWidget::NavigateDown()
{
    if (EntryCount == 0) { return; }

    BP_NavigateDown();
}

void UQuestLogWidget::NavigateUp()
{
    if (EntryCount == 0) { return; }

    BP_NavigateUp();
}

void UQuestLogWidget::ConfirmSelection()
{
    BP_Confirm();
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void UQuestLogWidget::HandleQuestStateChanged(FName QuestName, EQuestState NewState)
{
    UE_LOG(LogTemp, Log,
        TEXT("[QuestLogWidget] Quest '%s' state changed to %d — refreshing."),
        *QuestName.ToString(), (int32)NewState);

    RefreshQuestLog();
}

void UQuestLogWidget::HandleQuestStepAdvanced(FName QuestName, int32 StepIndex)
{
    UE_LOG(LogTemp, Log,
        TEXT("[QuestLogWidget] Quest '%s' step %d advanced — refreshing."),
        *QuestName.ToString(), StepIndex);

    RefreshQuestLog();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

UQuestData* UQuestLogWidget::LoadQuestAsset(FName QuestName) const
{
    const FString AssetPath = FString::Printf(TEXT("/Game/Data/QuestData/%s.%s"),
        *QuestName.ToString(), *QuestName.ToString());

    UQuestData* Asset = LoadObject<UQuestData>(nullptr, *AssetPath);
    if (!Asset)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[QuestLogWidget] Could not load QuestData asset at '%s'."), *AssetPath);
    }
    return Asset;
}