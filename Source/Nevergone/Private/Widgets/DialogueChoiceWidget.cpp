// Copyright Xyzto Works

#include "Widgets/DialogueChoiceWidget.h"
#include "World/DialogueSubsystem.h"

void UDialogueChoiceWidget::SetupChoice(int32 InIndex, const FText& InLabel, bool bInIsLocked, const FText& InLockedTooltip)
{
	ChoiceIndex   = InIndex;
	Label         = InLabel;
	bIsLocked     = bInIsLocked;
	LockedTooltip = InLockedTooltip;
}

void UDialogueChoiceWidget::OnClicked()
{
	if (bIsLocked)
	{
		UE_LOG(LogTemp, Log, TEXT("[DialogueChoiceWidget] Choice %d is locked — ignoring click."), ChoiceIndex);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI) { return; }

	UDialogueSubsystem* DialogueSys = GI->GetSubsystem<UDialogueSubsystem>();
	if (!DialogueSys) { return; }

	UE_LOG(LogTemp, Log, TEXT("[DialogueChoiceWidget] Choice %d clicked."), ChoiceIndex);
	DialogueSys->SelectChoice(ChoiceIndex);
}


bool UDialogueChoiceWidget::IsChoiceEnabled() const
{
	return !bIsLocked;
}