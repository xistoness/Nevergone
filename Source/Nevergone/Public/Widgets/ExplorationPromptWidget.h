// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ExplorationPromptWidget.generated.h"

// ---------------------------------------------------------------------------
// UExplorationPromptWidget
//
// Single-line transient message widget for the exploration HUD. Generic and
// reusable: doors use it for "It's locked.", later systems can use it for
// "Inventory full", "Picked up X", and similar one-off feedback.
//
// Authority split:
//   - C++ owns visibility (Collapsed <-> SelfHitTestInvisible) and timing.
//   - BP   owns animations (fade-in / fade-out playback only).
//
// The BP MUST NOT call SetVisibility on the widget root anywhere. Letting both
// sides toggle visibility creates a state race that breaks repeat shows.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UExplorationPromptWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    /**
     * Displays a one-line message for `Duration` seconds, then auto-hides.
     * Calling this while a previous prompt is still visible REPLACES the
     * message and restarts the timer (most-recent-wins).
     *
     * @param Message    Localized text to display.
     * @param Duration   Seconds before BP_HidePrompt is called. Clamped to >= 0.1s.
     */
    UFUNCTION(BlueprintCallable, Category = "ExplorationPrompt")
    void ShowMessage(const FText& Message, float Duration = 2.0f);

    /** Cancels any pending hide and collapses immediately, no fade-out. */
    UFUNCTION(BlueprintCallable, Category = "ExplorationPrompt")
    void HideImmediate();

protected:

    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    /**
     * BP implements: apply Message to the TextBlock and play the fade-in
     * animation. Visibility is already SelfHitTestInvisible by the time
     * this event fires.
     *
     * IMPORTANT: do NOT call SetVisibility from BP. Recommended first node
     * is "Stop All Animations" so a fade-out still in flight gets cancelled
     * before the new fade-in starts.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "ExplorationPrompt")
    void BP_ShowMessage(const FText& Message);

    /**
     * BP implements: play the fade-out animation only.
     *
     * IMPORTANT: do NOT call SetVisibility from BP and do NOT bind the
     * "Set Collapsed" action to OnAnimationFinished. C++ collapses the
     * widget after FadeOutDuration elapses.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "ExplorationPrompt")
    void BP_HidePrompt();

    /**
     * Length of the BP fade-out animation in seconds. C++ waits this long
     * after BP_HidePrompt before collapsing the widget. Match this to the
     * actual animation length in the BP Animation tab.
     */
    UPROPERTY(EditDefaultsOnly, Category = "ExplorationPrompt", meta = (ClampMin = "0.0"))
    float FadeOutDuration = 0.3f;

private:

    void HandleHideTimer();
    void HandleCollapseTimer();

    FTimerHandle HideTimerHandle;
    FTimerHandle CollapseTimerHandle;
};