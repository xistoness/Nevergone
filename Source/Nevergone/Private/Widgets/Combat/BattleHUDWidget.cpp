// Copyright Xyzto Works

#include "Widgets/Combat/BattleHUDWidget.h"

#include "Characters/CharacterBase.h"
#include "Components/VerticalBox.h"
#include "GameMode/CombatManager.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/BattleUnitState.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "GameMode/TurnManager.h"
#include "Widgets/Combat/BattleResultsWidget.h"
#include "Widgets/Combat/TurnIndicatorWidget.h"
#include "Widgets/Combat/UnitHPBarWidget.h"

void UBattleHUDWidget::InitializeWithCombatManager(UCombatManager* InCombatManager)
{
	if (!InCombatManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUDWidget] InitializeWithCombatManager: null CombatManager"));
		return;
	}

	CombatManager = InCombatManager;

	// Subscribe to combat end so we can show the results screen
	CombatManager->OnCombatFinished.AddDynamic(this, &UBattleHUDWidget::HandleCombatFinished);

	UBattleState*   BattleState  = CombatManager->GetBattleState();
	UTurnManager*   TurnManager  = CombatManager->GetTurnManager();
	UCombatEventBus* EventBus    = CombatManager->GetCombatEventBus();

	if (BattleState && EventBus)
	{
		SpawnHPBars(BattleState);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleHUDWidget] BattleState or EventBus null — HP bars skipped"));
	}

	if (TurnManager)
	{
		SpawnTurnIndicator(TurnManager);

		// Mirror current turn state immediately so the indicator is correct on spawn
		TurnStateHandle = TurnManager->OnTurnStateChanged.AddUObject(
			this, &UBattleHUDWidget::HandleTurnStateChanged);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleHUDWidget] TurnManager null — turn indicator skipped"));
	}

	UE_LOG(LogTemp, Log, TEXT("[BattleHUDWidget] Initialized with %d HP bars"), HPBarMap.Num());

	OnHUDInitialized();
}

void UBattleHUDWidget::Deinitialize()
{
	// Unsubscribe from combat finished
	if (CombatManager)
	{
		CombatManager->OnCombatFinished.RemoveDynamic(this, &UBattleHUDWidget::HandleCombatFinished);
	}

	// Unsubscribe from turn state
	if (CombatManager && CombatManager->GetTurnManager() && TurnStateHandle.IsValid())
	{
		CombatManager->GetTurnManager()->OnTurnStateChanged.Remove(TurnStateHandle);
		TurnStateHandle.Reset();
	}

	// Deinitialize and remove all HP bars
	for (auto& Pair : HPBarMap)
	{
		if (Pair.Value)
		{
			Pair.Value->Deinitialize();
			Pair.Value->RemoveFromParent();
		}
	}
	HPBarMap.Empty();

	// Deinitialize turn indicator
	if (TurnIndicatorInstance)
	{
		TurnIndicatorInstance->Deinitialize();
	}

	UE_LOG(LogTemp, Log, TEXT("[BattleHUDWidget] Deinitialized"));
}

// ---------------------------------------------------------------------------
// Internal setup
// ---------------------------------------------------------------------------

void UBattleHUDWidget::SpawnHPBars(UBattleState* BattleState)
{
	if (!HPBarWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleHUDWidget] HPBarWidgetClass not set — HP bars disabled"));
		return;
	}

	UCombatEventBus* EventBus = CombatManager->GetCombatEventBus();

	// Iterate all unit states and create one HP bar widget per unit
	for (const FBattleUnitState& UnitState : BattleState->GetAllUnitStates())
	{
		ACharacterBase* Unit = UnitState.UnitActor.Get();
		if (!IsValid(Unit))
		{
			continue;
		}

		UUnitHPBarWidget* HPBar = CreateWidget<UUnitHPBarWidget>(GetOwningPlayer(), HPBarWidgetClass);
		if (!HPBar)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BattleHUDWidget] Failed to create HPBar for %s"), *GetNameSafe(Unit));
			continue;
		}

		// Add to viewport at a high Z-order so it renders above world geometry
		HPBar->AddToViewport(10);
		HPBar->InitializeForUnit(Unit, EventBus, UnitState.MaxHP, UnitState.CurrentHP);

		HPBarMap.Add(Unit, HPBar);

		UE_LOG(LogTemp, Log, TEXT("[BattleHUDWidget] HP bar spawned for %s (HP %.0f/%.0f)"),
			*GetNameSafe(Unit), UnitState.CurrentHP, UnitState.MaxHP);
	}
}

void UBattleHUDWidget::SpawnTurnIndicator(UTurnManager* TurnManager)
{
	if (!TurnIndicatorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleHUDWidget] TurnIndicatorClass not set — turn indicator disabled"));
		return;
	}

	TurnIndicatorInstance = CreateWidget<UTurnIndicatorWidget>(GetOwningPlayer(), TurnIndicatorClass);
	if (!TurnIndicatorInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUDWidget] Failed to create TurnIndicatorWidget"));
		return;
	}

	if (TurnIndicatorContainer)
	{
		TurnIndicatorContainer->AddChild(TurnIndicatorInstance);
	}
	else
	{
		// Fallback: add directly to viewport if no container is bound
		TurnIndicatorInstance->AddToViewport(11);
	}

	TurnIndicatorInstance->InitializeWithTurnManager(TurnManager);

	UE_LOG(LogTemp, Log, TEXT("[BattleHUDWidget] Turn indicator spawned"));
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void UBattleHUDWidget::HandleCombatFinished(EBattleUnitTeam WinningTeam)
{
	if (!BattleResultsClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleHUDWidget] BattleResultsClass not set — results screen skipped"));
		return;
	}

	UBattleResultsWidget* Results = CreateWidget<UBattleResultsWidget>(GetOwningPlayer(), BattleResultsClass);
	if (!Results)
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUDWidget] Failed to create BattleResultsWidget"));
		return;
	}

	const int32 AliveAllies  = CombatManager ? CombatManager->GetAliveAllies()  : 0;
	const int32 AliveEnemies = CombatManager ? CombatManager->GetAliveEnemies() : 0;

	// Allies lost = total units initialized minus those still alive.
	// We expose this through BattleState's total count minus alive count.
	const int32 TotalAllies  = CombatManager ? CombatManager->GetTotalAllies()  : 0;
	const int32 AlliesLost   = FMath::Max(0, TotalAllies - AliveAllies);

	Results->AddToViewport(100); // Very high Z so it covers everything
	Results->ShowResults(WinningTeam, AliveAllies, AliveEnemies, AlliesLost);

	UE_LOG(LogTemp, Log,
		TEXT("[BattleHUDWidget] Results screen shown — Winner=%d, Allies=%d/%d, Enemies=%d"),
		(int32)WinningTeam, AliveAllies, TotalAllies, AliveEnemies);
}

void UBattleHUDWidget::HandleTurnStateChanged(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase)
{
	const bool bIsPlayerTurn = (NewOwner == EBattleTurnOwner::Player);
	OnTurnChanged(bIsPlayerTurn);
}