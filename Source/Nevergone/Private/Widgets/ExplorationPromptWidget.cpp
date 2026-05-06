// Copyright Xyzto Works

#include "Widgets/ExplorationPromptWidget.h"

#include "TimerManager.h"

void UExplorationPromptWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // Start collapsed. ShowMessage is the only path that flips visibility back on.
    SetVisibility(ESlateVisibility::Collapsed);

    UE_LOG(LogTemp, Verbose, TEXT("[ExplorationPromptWidget] NativeOnInitialized: collapsed."));
}

void UExplorationPromptWidget::ShowMessage(const FText& Message, float Duration)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ExplorationPromptWidget] ShowMessage: no UWorld — message '%s' dropped."),
            *Message.ToString());
        return;
    }

    const float SafeDuration = FMath::Max(Duration, 0.1f);

    UE_LOG(LogTemp, Log,
        TEXT("[ExplorationPromptWidget] ShowMessage: '%s' duration=%.2fs"),
        *Message.ToString(), SafeDuration);

    // Cancel any pending callbacks from a previous prompt cycle. Both timers
    // must be cleared — otherwise a leftover collapse timer would fire mid-show.
    FTimerManager& TM = World->GetTimerManager();
    TM.ClearTimer(HideTimerHandle);
    TM.ClearTimer(CollapseTimerHandle);

    // Force a clean visible state BEFORE the BP fade-in plays. Two cases this
    // recovers from:
    //   1. Widget is Collapsed after a previous fade-out completed — without
    //      this line the fade-in animation would play against a hidden widget.
    //   2. Widget is mid-fadeout — flipping visibility on is harmless; the BP
    //      "Stop All Animations" call cancels the in-flight fade-out before
    //      starting the new fade-in.
    // SelfHitTestInvisible: rendered, but doesn't capture input — informational
    // prompts must never block clicks/gamepad on whatever is underneath.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    BP_ShowMessage(Message);

    TM.SetTimer(HideTimerHandle, this,
        &UExplorationPromptWidget::HandleHideTimer,
        SafeDuration,
        /*bLoop=*/false);
}

void UExplorationPromptWidget::HideImmediate()
{
    UWorld* World = GetWorld();
    if (World)
    {
        FTimerManager& TM = World->GetTimerManager();
        TM.ClearTimer(HideTimerHandle);
        TM.ClearTimer(CollapseTimerHandle);
    }

    UE_LOG(LogTemp, Verbose, TEXT("[ExplorationPromptWidget] HideImmediate."));

    // Skip the fade-out — go straight to collapsed.
    SetVisibility(ESlateVisibility::Collapsed);
}

void UExplorationPromptWidget::NativeDestruct()
{
    // Defensive cleanup — if the widget is torn down while timers are pending
    // (level transition, controller unpossess), avoid stale callbacks.
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TM = World->GetTimerManager();
        TM.ClearTimer(HideTimerHandle);
        TM.ClearTimer(CollapseTimerHandle);
    }

    Super::NativeDestruct();
}

void UExplorationPromptWidget::HandleHideTimer()
{
    UE_LOG(LogTemp, Verbose, TEXT("[ExplorationPromptWidget] HandleHideTimer fired — playing fade-out."));

    BP_HidePrompt();

    // Schedule the actual collapse for after the fade-out animation finishes.
    // BP is forbidden from collapsing the widget itself — see the contract on
    // BP_HidePrompt in the header.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(CollapseTimerHandle, this,
            &UExplorationPromptWidget::HandleCollapseTimer,
            FMath::Max(FadeOutDuration, 0.01f),
            /*bLoop=*/false);
    }
}

void UExplorationPromptWidget::HandleCollapseTimer()
{
    UE_LOG(LogTemp, Verbose, TEXT("[ExplorationPromptWidget] HandleCollapseTimer fired — collapsing."));
    SetVisibility(ESlateVisibility::Collapsed);
}