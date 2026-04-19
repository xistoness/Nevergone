// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/DialogueSystemTypes.h"
#include "DialogueWidget.generated.h"

class UDialogueSubsystem;

/**
 * UDialogueWidget
 *
 * Reactive dialogue UI with manual selection state.
 */
UCLASS()
class NEVERGONE_API UDialogueWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void OnChoiceSelected(int32 ChoiceIndex);

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void OnConfirm();

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void SelectNextChoice();

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void SelectPreviousChoice();

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void ConfirmCurrentChoice();

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void CancelDialogue();

    UFUNCTION(BlueprintPure, Category = "Dialogue")
    bool HasSelectableChoices() const;
    
    UFUNCTION(BlueprintPure, Category = "Dialogue")
    int32 GetCurrentSelectedChoiceIndex() const { return CurrentSelectedChoiceIndex; }

protected:

    UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue")
    void BP_ShowNode(const FText& SpeakerName, const FText& Body, UTexture2D* Portrait);

    UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue")
    void BP_AddChoice(int32 Index, const FText& Label, bool bIsLocked, const FText& LockedTooltip);

    UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue")
    void BP_ClearChoices();

    UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue")
    void BP_Hide();

    // Blueprint should update the visual highlight for the selected choice index.
    // If SelectedIndex == INDEX_NONE, clear all highlight.
    UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue")
    void BP_SetSelectedChoice(int32 SelectedIndex);

private:

    void HandleNodeReady(const FDialogueNode& Node);
    void HandleDialogueStarted();
    void HandleDialogueEnded();

    void RefreshSelectionVisual();
    void RebuildSelectableChoiceCache(const FDialogueNode& Node);
    void SelectFirstAvailableChoice();

private:

    UPROPERTY()
    UDialogueSubsystem* DialogueSubsystem = nullptr;

    UPROPERTY()
    TArray<int32> SelectableChoiceIndices;

    int32 CurrentSelectedChoiceIndex = INDEX_NONE;
    bool bCurrentNodeHasChoices = false;
};