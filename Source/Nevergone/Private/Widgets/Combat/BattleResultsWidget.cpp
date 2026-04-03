// Copyright Xyzto Works

#include "Widgets/Combat/BattleResultsWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UBattleResultsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind the continue button click — the widget is always constructed before ShowResults
	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddDynamic(this, &UBattleResultsWidget::HandleContinueClicked);
	}
}

void UBattleResultsWidget::ShowResults(EBattleUnitTeam WinningTeam,
										int32 SurvivingAllies,
										int32 SurvivingEnemies,
										int32 AlliesLost)
{
	const bool bIsVictory = (WinningTeam == EBattleUnitTeam::Ally);

	// Populate header
	if (ResultHeaderText)
	{
		ResultHeaderText->SetText(FText::FromString(bIsVictory ? TEXT("VICTORY") : TEXT("DEFEAT")));
	}

	// Populate stat lines
	if (SurvivingAlliesText)
	{
		SurvivingAlliesText->SetText(FText::FromString(
			FString::Printf(TEXT("Surviving Allies: %d"), SurvivingAllies)));
	}

	if (AlliesLostText)
	{
		AlliesLostText->SetText(FText::FromString(
			FString::Printf(TEXT("Allies Lost: %d"), AlliesLost)));
	}

	// Notify Blueprint to play any entry animation
	OnResultsReady(bIsVictory);

	UE_LOG(LogTemp, Log,
		TEXT("[BattleResultsWidget] ShowResults — Victory=%s | Allies=%d | Lost=%d | EnemiesLeft=%d"),
		bIsVictory ? TEXT("YES") : TEXT("NO"),
		SurvivingAllies, AlliesLost, SurvivingEnemies);
}

void UBattleResultsWidget::HandleContinueClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[BattleResultsWidget] Continue button clicked"));

	OnContinueClicked.Broadcast();

	// Remove ourselves — the GameContextManager drives the actual transition
	RemoveFromParent();
}