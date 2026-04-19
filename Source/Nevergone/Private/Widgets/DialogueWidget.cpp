// Copyright Xyzto Works

#include "Widgets/DialogueWidget.h"

#include "World/DialogueSubsystem.h"

void UDialogueWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UGameInstance* GI = GetGameInstance();
    if (!GI) { return; }

    DialogueSubsystem = GI->GetSubsystem<UDialogueSubsystem>();
    if (!DialogueSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[DialogueWidget] Failed to find DialogueSubsystem."));
        return;
    }

    DialogueSubsystem->OnDialogueStarted.AddUObject(this, &UDialogueWidget::HandleDialogueStarted);
    DialogueSubsystem->OnDialogueNodeReady.AddUObject(this, &UDialogueWidget::HandleNodeReady);
    DialogueSubsystem->OnDialogueEnded.AddUObject(this, &UDialogueWidget::HandleDialogueEnded);

    UE_LOG(LogTemp, Log, TEXT("[DialogueWidget] Constructed and bound to DialogueSubsystem."));
}

void UDialogueWidget::NativeDestruct()
{
    if (DialogueSubsystem)
    {
        DialogueSubsystem->OnDialogueStarted.RemoveAll(this);
        DialogueSubsystem->OnDialogueNodeReady.RemoveAll(this);
        DialogueSubsystem->OnDialogueEnded.RemoveAll(this);
    }

    Super::NativeDestruct();
}

void UDialogueWidget::OnChoiceSelected(int32 ChoiceIndex)
{
    if (!DialogueSubsystem) { return; }

    UE_LOG(LogTemp, Log, TEXT("[DialogueWidget] Player selected choice %d."), ChoiceIndex);
    DialogueSubsystem->SelectChoice(ChoiceIndex);
}

void UDialogueWidget::OnConfirm()
{
    if (!DialogueSubsystem) { return; }
    DialogueSubsystem->SelectChoice(0);
}

void UDialogueWidget::SelectNextChoice()
{
    if (SelectableChoiceIndices.Num() == 0)
    {
        return;
    }

    int32 CurrentSelectableArrayIndex = SelectableChoiceIndices.IndexOfByKey(CurrentSelectedChoiceIndex);
    if (CurrentSelectableArrayIndex == INDEX_NONE)
    {
        CurrentSelectedChoiceIndex = SelectableChoiceIndices[0];
    }
    else
    {
        CurrentSelectableArrayIndex = (CurrentSelectableArrayIndex + 1) % SelectableChoiceIndices.Num();
        CurrentSelectedChoiceIndex = SelectableChoiceIndices[CurrentSelectableArrayIndex];
    }

    RefreshSelectionVisual();
}

void UDialogueWidget::SelectPreviousChoice()
{
    if (SelectableChoiceIndices.Num() == 0)
    {
        return;
    }

    int32 CurrentSelectableArrayIndex = SelectableChoiceIndices.IndexOfByKey(CurrentSelectedChoiceIndex);
    if (CurrentSelectableArrayIndex == INDEX_NONE)
    {
        CurrentSelectedChoiceIndex = SelectableChoiceIndices.Last();
    }
    else
    {
        CurrentSelectableArrayIndex =
            (CurrentSelectableArrayIndex - 1 + SelectableChoiceIndices.Num()) % SelectableChoiceIndices.Num();
        CurrentSelectedChoiceIndex = SelectableChoiceIndices[CurrentSelectableArrayIndex];
    }

    RefreshSelectionVisual();
}

void UDialogueWidget::ConfirmCurrentChoice()
{
    if (!DialogueSubsystem) { return; }

    if (!bCurrentNodeHasChoices)
    {
        DialogueSubsystem->EndDialogue();
        return;
    }

    if (CurrentSelectedChoiceIndex == INDEX_NONE)
    {
        SelectFirstAvailableChoice();
    }

    if (CurrentSelectedChoiceIndex != INDEX_NONE)
    {
        DialogueSubsystem->SelectChoice(CurrentSelectedChoiceIndex);
    }
}

void UDialogueWidget::CancelDialogue()
{
    if (!DialogueSubsystem) { return; }
    DialogueSubsystem->EndDialogue();
}

bool UDialogueWidget::HasSelectableChoices() const
{
    return SelectableChoiceIndices.Num() > 0;
}

void UDialogueWidget::HandleDialogueStarted()
{
    SetVisibility(ESlateVisibility::Visible);
}

void UDialogueWidget::HandleNodeReady(const FDialogueNode& Node)
{
    UE_LOG(LogTemp, Log, TEXT("[DialogueWidget] Received NodeReady broadcast"));

    BP_ShowNode(Node.SpeakerName, Node.Body, Node.SpeakerPortrait);
    BP_ClearChoices();

    bCurrentNodeHasChoices = Node.Choices.Num() > 0;

    for (int32 i = 0; i < Node.Choices.Num(); ++i)
    {
        const FDialogueChoice& Choice = Node.Choices[i];
        const bool bLocked = !DialogueSubsystem->EvaluateCondition(Choice.Condition);

        UE_LOG(LogTemp, Log, TEXT("[DialogueWidget] Adding choice %d (Locked=%d)."), i, bLocked ? 1 : 0);
        BP_AddChoice(i, Choice.Label, bLocked, Choice.LockedTooltip);
    }

    RebuildSelectableChoiceCache(Node);
    SelectFirstAvailableChoice();
    RefreshSelectionVisual();
}

void UDialogueWidget::HandleDialogueEnded()
{
    UE_LOG(LogTemp, Log, TEXT("[DialogueWidget] Dialogue ended — hiding widget."));

    SelectableChoiceIndices.Reset();
    CurrentSelectedChoiceIndex = INDEX_NONE;
    bCurrentNodeHasChoices = false;

    BP_SetSelectedChoice(INDEX_NONE);
    BP_Hide();
}

void UDialogueWidget::RefreshSelectionVisual()
{
    BP_SetSelectedChoice(CurrentSelectedChoiceIndex);
}

void UDialogueWidget::RebuildSelectableChoiceCache(const FDialogueNode& Node)
{
    SelectableChoiceIndices.Reset();

    for (int32 i = 0; i < Node.Choices.Num(); ++i)
    {
        const FDialogueChoice& Choice = Node.Choices[i];
        if (DialogueSubsystem->EvaluateCondition(Choice.Condition))
        {
            SelectableChoiceIndices.Add(i);
        }
    }
}

void UDialogueWidget::SelectFirstAvailableChoice()
{
    CurrentSelectedChoiceIndex =
        SelectableChoiceIndices.Num() > 0
        ? SelectableChoiceIndices[0]
        : INDEX_NONE;
}