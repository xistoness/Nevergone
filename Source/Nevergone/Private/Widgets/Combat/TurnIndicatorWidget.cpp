// Copyright Xyzto Works

#include "Widgets/Combat/TurnIndicatorWidget.h"

#include "Components/TextBlock.h"
#include "GameMode/TurnManager.h"

// Localization namespace for all gathered strings in this translation unit.
// The Localization Dashboard groups text by namespace + key — keep the namespace
// stable so existing translations are never invalidated.
#define LOCTEXT_NAMESPACE "TurnIndicator"

void UTurnIndicatorWidget::InitializeWithTurnManager(UTurnManager* InTurnManager)
{
	if (!InTurnManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[TurnIndicatorWidget] InitializeWithTurnManager: null TurnManager"));
		return;
	}

	TurnManager = InTurnManager;

	TurnHandle = InTurnManager->OnTurnStateChanged.AddUObject(
		this, &UTurnIndicatorWidget::HandleTurnStateChanged);

	// Reflect the current state immediately so the widget is correct at spawn
	const bool bIsPlayer = (InTurnManager->GetCurrentTurnOwner() == EBattleTurnOwner::Player);
	if (TurnLabel)
	{
		TurnLabel->SetText(bIsPlayer
			? LOCTEXT("PlayerTurnLabel", "PLAYER TURN")
			: LOCTEXT("EnemyTurnLabel",  "ENEMY TURN"));
	}
	OnTurnOwnerChanged(bIsPlayer);

	UE_LOG(LogTemp, Log, TEXT("[TurnIndicatorWidget] Initialized — current owner: %s"),
		bIsPlayer ? TEXT("Player") : TEXT("Enemy"));
}

void UTurnIndicatorWidget::Deinitialize()
{
	if (TurnManager && TurnHandle.IsValid())
	{
		TurnManager->OnTurnStateChanged.Remove(TurnHandle);
		TurnHandle.Reset();
	}

	TurnManager = nullptr;
}

void UTurnIndicatorWidget::HandleTurnStateChanged(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase)
{
	// Only react on AwaitingOrders — this is the stable "turn started" phase
	if (NewPhase != EBattleTurnPhase::AwaitingOrders)
	{
		return;
	}

	const bool bIsPlayerTurn = (NewOwner == EBattleTurnOwner::Player);

	if (TurnLabel)
	{
		TurnLabel->SetText(bIsPlayerTurn
			? LOCTEXT("PlayerTurnLabel", "PLAYER TURN")
			: LOCTEXT("EnemyTurnLabel",  "ENEMY TURN"));
	}

	OnTurnOwnerChanged(bIsPlayerTurn);

	UE_LOG(LogTemp, Log, TEXT("[TurnIndicatorWidget] Turn changed — %s"),
		bIsPlayerTurn ? TEXT("Player") : TEXT("Enemy"));
}

#undef LOCTEXT_NAMESPACE
