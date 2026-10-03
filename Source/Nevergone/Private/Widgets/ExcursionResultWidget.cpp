// Copyright Xyzto Works

#include "Widgets/ExcursionResultWidget.h"

#include "Nevergone.h"
#include "Data/ItemDefinition.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

// Localization namespace for all gathered strings in this translation unit.
#define LOCTEXT_NAMESPACE "ExcursionResult"

void UExcursionResultWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Bound once here rather than in NativeConstruct, which runs again every
	// time the widget is added to the viewport.
	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddDynamic(this, &UExcursionResultWidget::HandleContinueClicked);
	}
}

void UExcursionResultWidget::ShowResult(const FExcursionResult& Result)
{
	bContinued = false;

	const bool bSurvived = Result.EndReason == EExcursionEndReason::Exited;

	if (HeaderText)
	{
		HeaderText->SetText(bSurvived
			? LOCTEXT("ExitedHeader", "Back from the excursion")
			: LOCTEXT("KilledHeader", "The survivor did not come back"));
	}

	if (ConditionText)
	{
		FText Condition;

		switch (Result.Condition)
		{
		case EExcursionSurvivorCondition::Infected:
			Condition = LOCTEXT("ConditionInfected", "Infected");
			break;

		case EExcursionSurvivorCondition::Dead:
			Condition = LOCTEXT("ConditionDead", "Dead");
			break;

		default:
			Condition = LOCTEXT("ConditionUnharmed", "Unharmed");
			break;
		}

		if (Result.SafeguardsUsed > 0)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Condition"), Condition);
			Args.Add(TEXT("Count"), FText::AsNumber(Result.SafeguardsUsed));

			Condition = FText::Format(LOCTEXT("ConditionWithSafeguards", "{Condition}\nSafeguards used: {Count}"), Args);
		}

		ConditionText->SetText(Condition);
	}

	if (SurvivorsText)
	{
		TArray<FText> Names;

		for (const FText& Name : Result.RescuedSurvivors)
		{
			Names.Add(Name.IsEmpty() ? LOCTEXT("UnnamedSurvivor", "Unnamed survivor") : Name);
		}

		SurvivorsText->SetText(Names.IsEmpty() ? LOCTEXT("NoneRescued", "No one") : JoinLines(Names));
	}

	if (ItemsFoundLabel)
	{
		ItemsFoundLabel->SetText(bSurvived
			? LOCTEXT("ItemsFoundLabel", "Resources found")
			: LOCTEXT("ItemsLostLabel", "Resources lost"));
	}

	if (ItemsFoundText)
	{
		ItemsFoundText->SetText(DescribeItems(Result.ItemsFound));
	}

	if (ItemsSpentText)
	{
		ItemsSpentText->SetText(DescribeItems(Result.ItemsSpent));
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionResultWidget] %s showing result (survived %d)"),
		*GetNameSafe(this), bSurvived ? 1 : 0);

	OnResultShown(bSurvived);
}

void UExcursionResultWidget::Confirm()
{
	HandleContinueClicked();
}

void UExcursionResultWidget::HandleContinueClicked()
{
	if (bContinued)
	{
		return;
	}

	bContinued = true;

	OnContinue.Broadcast();
}

FText UExcursionResultWidget::DescribeItems(const TArray<FExcursionItemCount>& Items)
{
	if (Items.IsEmpty())
	{
		return LOCTEXT("NoItems", "None");
	}

	TArray<FText> Lines;

	for (const FExcursionItemCount& Count : Items)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Item"), Count.Item ? Count.Item->DisplayName : LOCTEXT("UnknownItem", "Unknown item"));
		Args.Add(TEXT("Quantity"), FText::AsNumber(Count.Quantity));

		Lines.Add(FText::Format(LOCTEXT("ItemLine", "{Item} x{Quantity}"), Args));
	}

	return JoinLines(Lines);
}

FText UExcursionResultWidget::JoinLines(const TArray<FText>& Lines)
{
	// A line break is layout, not language: culture-invariant on purpose.
	return FText::Join(INVTEXT("\n"), Lines);
}

#undef LOCTEXT_NAMESPACE
