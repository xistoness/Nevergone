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
										int32 EnemiesKilled)
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

	if (EnemiesKilledText)
	{
		EnemiesKilledText->SetText(FText::FromString(
			FString::Printf(TEXT("Enemies Killed: %d"), EnemiesKilled)));
	}

	// Notify Blueprint to play any entry animation
	OnResultsReady(bIsVictory);

	UE_LOG(LogTemp, Log,
		TEXT("[BattleResultsWidget] ShowResults — Victory=%s | Allies=%d | Killed=%d | EnemiesLeft=%d"),
		bIsVictory ? TEXT("YES") : TEXT("NO"),
		SurvivingAllies, EnemiesKilled, SurvivingEnemies);
}

void UBattleResultsWidget::HandleContinueClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[BattleResultsWidget] Continue button clicked"));

	OnContinueClicked.Broadcast();

	// Remove ourselves — the GameContextManager drives the actual transition
	RemoveFromParent();
}