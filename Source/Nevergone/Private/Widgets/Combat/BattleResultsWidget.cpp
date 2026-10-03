// Copyright Xyzto Works

#include "Widgets/Combat/BattleResultsWidget.h"

#include "Audio/AudioSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

// Localization namespace for all gathered strings in this translation unit.
#define LOCTEXT_NAMESPACE "BattleResults"

class UAudioSubsystem;

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
		ResultHeaderText->SetText(bIsVictory
			? LOCTEXT("VictoryHeader", "VICTORY")
			: LOCTEXT("DefeatHeader",  "DEFEAT"));
	}

	// Populate stat lines.
	// FText::Format with a named argument keeps the placeholder localizable —
	// translators can rearrange "{Count}" wherever the target language reads
	// most naturally (e.g. "Inimigos abatidos: {Count}").
	if (SurvivingAlliesText)
	{
		SurvivingAlliesText->SetText(FText::Format(
			LOCTEXT("SurvivingAlliesFormat", "Surviving Allies: {Count}"),
			FText::AsNumber(SurvivingAllies)));
	}

	if (EnemiesKilledText)
	{
		EnemiesKilledText->SetText(FText::Format(
			LOCTEXT("EnemiesKilledFormat", "Enemies Killed: {Count}"),
			FText::AsNumber(EnemiesKilled)));
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
	
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
		{
			Audio->PlayUISoundEvent(EUISoundEvent::ButtonConfirm);
		}
	}

	// Remove ourselves — the GameContextManager drives the actual transition
	RemoveFromParent();
}

#undef LOCTEXT_NAMESPACE
