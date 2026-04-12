// Copyright Xyzto Works

#include "Widgets/Combat/BattleHUDWidget.h"

#include "Characters/CharacterBase.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GameMode/CombatManager.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/BattleUnitState.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "GameMode/TurnManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"
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
	CombatManager->OnCombatFinished.AddDynamic(this, &UBattleHUDWidget::HandleCombatFinished);

	UBattleState*    BattleState = CombatManager->GetBattleState();
	UTurnManager*    TurnManager = CombatManager->GetTurnManager();
	UCombatEventBus* EventBus   = CombatManager->GetCombatEventBus();

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
		TurnStateHandle = TurnManager->OnTurnStateChanged.AddUObject(
			this, &UBattleHUDWidget::HandleTurnStateChanged);
	}

	UE_LOG(LogTemp, Log, TEXT("[BattleHUDWidget] Initialized with %d HP bars"), HPBarWidgets.Num());
	OnHUDInitialized();
}

void UBattleHUDWidget::Deinitialize()
{
	if (CombatManager)
	{
		CombatManager->OnCombatFinished.RemoveDynamic(this, &UBattleHUDWidget::HandleCombatFinished);

		if (UTurnManager* TM = CombatManager->GetTurnManager())
		{
			if (TurnStateHandle.IsValid())
			{
				TM->OnTurnStateChanged.Remove(TurnStateHandle);
				TurnStateHandle.Reset();
			}
		}
	}

	for (UUnitHPBarWidget* Bar : HPBarWidgets)
	{
		if (Bar) { Bar->Deinitialize(); }
	}
	HPBarWidgets.Empty();
	HPBarUnits.Empty();

	if (TurnIndicatorInstance)
	{
		TurnIndicatorInstance->Deinitialize();
	}

	UE_LOG(LogTemp, Log, TEXT("[BattleHUDWidget] Deinitialized"));
}

// ---------------------------------------------------------------------------
// NativeTick — reposition every HP bar each frame using its CanvasPanelSlot
// ---------------------------------------------------------------------------

void UBattleHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!HPBarCanvas) { return; }

	APlayerController* PC = GetOwningPlayer();
	if (!PC) { return; }

	const float DPIScale = UWidgetLayoutLibrary::GetViewportScale(this);

	for (int32 i = 0; i < HPBarUnits.Num(); ++i)
	{
		ACharacterBase* Unit  = HPBarUnits[i].Get();
		UUnitHPBarWidget* Bar = HPBarWidgets[i].Get();

		if (!IsValid(Unit) || !Bar) { continue; }

		// Keep dead units hidden regardless of camera position
		if (CombatManager && CombatManager->GetBattleState())
		{
			const FBattleUnitState* State = CombatManager->GetBattleState()->FindUnitState(Unit);
			if (State && State->bIsDead)
			{
				Bar->SetVisibility(ESlateVisibility::Hidden);
				continue;
			}
		}

		const FVector WorldPos = Unit->GetActorLocation() + FVector(0.f, 0.f, HPBarWorldZOffset);

		FVector2D ScreenPos;
		if (!PC->ProjectWorldLocationToScreen(WorldPos, ScreenPos, false))
		{
			Bar->SetVisibility(ESlateVisibility::Hidden);
			continue;
		}

		// Dot product check — hide if unit is behind the camera
		FVector CamLoc;
		FRotator CamRot;
		PC->GetPlayerViewPoint(CamLoc, CamRot);
		if (FVector::DotProduct(CamRot.Vector(), (WorldPos - CamLoc).GetSafeNormal()) <= 0.f)
		{
			Bar->SetVisibility(ESlateVisibility::Hidden);
			continue;
		}

		Bar->SetVisibility(ESlateVisibility::HitTestInvisible);

		// Convert pixels to DPI-independent canvas coordinates
		const FVector2D CanvasPos = (ScreenPos / DPIScale) - (HPBarSize * 0.5f);

		// Move the slot — this is guaranteed to work because we added the bar
		// as a child of HPBarCanvas, so its slot is always a UCanvasPanelSlot
		if (UCanvasPanelSlot* HPSlot = Cast<UCanvasPanelSlot>(Bar->Slot))
		{
			HPSlot->SetPosition(CanvasPos);
			HPSlot->SetSize(HPBarSize);
		}
	}
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

	if (!HPBarCanvas)
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUDWidget] HPBarCanvas is null — add a CanvasPanel named 'HPBarCanvas' to WBP_BattleHUD"));
		return;
	}

	UCombatEventBus* EventBus = CombatManager->GetCombatEventBus();

	for (const FBattleUnitState& UnitState : BattleState->GetAllUnitStates())
	{
		ACharacterBase* Unit = UnitState.UnitActor.Get();
		if (!IsValid(Unit)) { continue; }

		UUnitHPBarWidget* Bar = CreateWidget<UUnitHPBarWidget>(GetOwningPlayer(), HPBarWidgetClass);
		if (!Bar)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BattleHUDWidget] Failed to create HPBar for %s"), *GetNameSafe(Unit));
			continue;
		}

		// Add as child of HPBarCanvas — this gives us a UCanvasPanelSlot we can reposition each tick
		UCanvasPanelSlot* HPSlot = HPBarCanvas->AddChildToCanvas(Bar);
		if (HPSlot)
		{
			HPSlot->SetSize(HPBarSize);
			HPSlot->SetPosition(FVector2D::ZeroVector); // Will be corrected on first tick
		}

		// Start hidden — first tick will show it if it's on screen
		Bar->SetVisibility(ESlateVisibility::Hidden);
		Bar->InitializeForUnit(Unit, EventBus, UnitState.MaxHP, UnitState.CurrentHP);

		// If this unit already has active status effects (mid-combat restore path),
		// replay the icon events immediately — they were broadcast before this
		// widget existed and were therefore missed by the event bus subscription.
		if (!UnitState.ActiveStatusEffects.IsEmpty())
		{
			Bar->SyncInitialStatusIcons(UnitState.ActiveStatusEffects);
		}

		HPBarUnits.Add(Unit);
		HPBarWidgets.Add(Bar);

		UE_LOG(LogTemp, Log, TEXT("[BattleHUDWidget] HP bar spawned for %s"), *GetNameSafe(Unit));
	}
}

void UBattleHUDWidget::SpawnTurnIndicator(UTurnManager* InTurnManager)
{
	if (!TurnIndicatorClass) { return; }

	TurnIndicatorInstance = CreateWidget<UTurnIndicatorWidget>(GetOwningPlayer(), TurnIndicatorClass);
	if (!TurnIndicatorInstance) { return; }

	// Add directly to this widget's canvas at a fixed position, or to the viewport
	TurnIndicatorInstance->AddToViewport(11);
	TurnIndicatorInstance->InitializeWithTurnManager(InTurnManager);

	UE_LOG(LogTemp, Log, TEXT("[BattleHUDWidget] Turn indicator spawned"));
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void UBattleHUDWidget::HandleCombatFinished(EBattleUnitTeam WinningTeam)
{
    // Results display is handled by BattleResultsController, which is spawned
    // by TowerFloorGameMode when GameContextManager enters the BattleResults state.
    // This widget must NOT create a BattleResultsWidget here because:
    //   1. It would have no bind to OnContinueClicked, so Continue does nothing.
    //   2. BattleResultsController creates its own widget with the correct bind.
    //   3. SwapPlayerControllers destroys this HUD's owning controller shortly after,
    //      which would destroy the widget before the player can interact with it.
    UE_LOG(LogTemp, Log,
        TEXT("[BattleHUDWidget] Combat finished (Winner=%d) — results handed off to BattleResultsController"),
        (int32)WinningTeam);
}

void UBattleHUDWidget::HandleTurnStateChanged(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase)
{
	OnTurnChanged(NewOwner == EBattleTurnOwner::Player);
}