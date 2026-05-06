// Copyright Xyzto Works

#include "Characters/PlayerControllers/BattlePreparationController.h"

#include "EnhancedInputComponent.h"
#include "GameInstance/GameContextManager.h"
#include "GameMode/BattlePreparationContext.h"
#include "Widgets/Combat/BattlePreparationWidget.h"
#include "Widgets/InventoryWidget.h"

// ---------------------------------------------------------------------------
// Input setup
// ---------------------------------------------------------------------------

void ABattlePreparationController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (ConfirmInput)
		{
			EIC->BindAction(ConfirmInput, ETriggerEvent::Triggered,
				this, &ABattlePreparationController::OnConfirm);
		}

		if (CancelInput)
		{
			EIC->BindAction(CancelInput, ETriggerEvent::Triggered,
				this, &ABattlePreparationController::OnCancel);
		}

		if (InventoryInput)
		{
			EIC->BindAction(InventoryInput, ETriggerEvent::Triggered,
				this, &ABattlePreparationController::OnInventory);
		}

		if (InventoryPrevCharInput)
		{
			EIC->BindAction(InventoryPrevCharInput, ETriggerEvent::Triggered,
				this, &ABattlePreparationController::OnInventoryPrevChar);
		}

		if (InventoryNextCharInput)
		{
			EIC->BindAction(InventoryNextCharInput, ETriggerEvent::Triggered,
				this, &ABattlePreparationController::OnInventoryNextChar);
		}

		if (UIUpInput)
			EIC->BindAction(UIUpInput, ETriggerEvent::Triggered, this, &ABattlePreparationController::OnInventoryNavigateUp);
		if (UIDownInput)
			EIC->BindAction(UIDownInput, ETriggerEvent::Triggered, this, &ABattlePreparationController::OnInventoryNavigateDown);
		if (UILeftInput)
			EIC->BindAction(UILeftInput, ETriggerEvent::Triggered, this, &ABattlePreparationController::OnInventoryNavigateLeft);
		if (UIRightInput)
			EIC->BindAction(UIRightInput, ETriggerEvent::Triggered, this, &ABattlePreparationController::OnInventoryNavigateRight);
		if (UIConfirmInput)
			EIC->BindAction(UIConfirmInput, ETriggerEvent::Triggered, this, &ABattlePreparationController::OnInventoryConfirm);
		if (UICancelInput)
			EIC->BindAction(UICancelInput, ETriggerEvent::Triggered, this, &ABattlePreparationController::OnInventoryCancel);
	}
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ABattlePreparationController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	// ReceivedPlayer fires after the engine assigns LocalPlayer to this controller.
	// This is the correct place for SetInputMode and CreateWidget calls, because
	// SpawnActor triggers BeginPlay BEFORE SwapPlayerControllers runs — meaning
	// LocalPlayer would be null in BeginPlay, causing SetInputMode to crash/teardown
	// the PIE session.
	if (!GetLocalPlayer())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[BattlePreparationController] ReceivedPlayer: LocalPlayer is still null — skipping UI setup."));
		return;
	}

	// Show cursor — the preparation screen is fully UI-driven.
	bShowMouseCursor = true;
	SetInputMode(FInputModeUIOnly());

	CreateInventoryWidget();

	UE_LOG(LogTemp, Log, TEXT("[BattlePreparationController] ReceivedPlayer: UI setup complete."));
}

void ABattlePreparationController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ABattlePreparationController::SetPreparationContext(UBattlePreparationContext* InContext)
{
	if (!InContext)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[BattlePreparationController] SetPreparationContext: null context"));
		return;
	}

	PreparationContext = InContext;

	UE_LOG(LogTemp, Log,
		TEXT("[BattlePreparationController] PreparationContext received — showing widget"));

	ShowPreparationWidget();
}

// ---------------------------------------------------------------------------
// Private — widget management
// ---------------------------------------------------------------------------

void ABattlePreparationController::ShowPreparationWidget()
{
	if (!PreparationContext) { return; }

	if (!PreparationWidgetClass)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[BattlePreparationController] PreparationWidgetClass not set — assign in BP CDO"));
		return;
	}

	if (!GetLocalPlayer())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[BattlePreparationController] No LocalPlayer — controller swap may not have completed"));
		return;
	}

	PreparationWidget = CreateWidget<UBattlePreparationWidget>(this, PreparationWidgetClass);
	if (!PreparationWidget)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[BattlePreparationController] CreateWidget failed for PreparationWidget"));
		return;
	}

	// Bind the widget's delegate before adding to viewport so no click is missed.
	PreparationWidget->OnStartBattleClicked.AddUObject(
		this, &ABattlePreparationController::HandleStartBattleClicked);
	PreparationWidget->OnAbortBattleClicked.AddUObject(
	this, &ABattlePreparationController::HandleAbortBattleClicked);

	PreparationWidget->PopulateFromContext(PreparationContext);
	PreparationWidget->AddToViewport();

	UE_LOG(LogTemp, Log, TEXT("[BattlePreparationController] Preparation widget shown"));
}

// ---------------------------------------------------------------------------
// Private — battle start / abort
// ---------------------------------------------------------------------------

void ABattlePreparationController::SendRequestBattleStart()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance) { return; }

	UGameContextManager* ContextManager =
		GameInstance->GetSubsystem<UGameContextManager>();

	if (!ContextManager) { return; }

	UE_LOG(LogTemp, Log,
		TEXT("[BattlePreparationController] Requesting battle start"));

	ContextManager->RequestBattleStart();
}

void ABattlePreparationController::SendRequestAbortBattle()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance) { return; }

	UGameContextManager* ContextManager =
		GameInstance->GetSubsystem<UGameContextManager>();

	if (!ContextManager) { return; }

	UE_LOG(LogTemp, Log,
		TEXT("[BattlePreparationController] Requesting battle start"));

	ContextManager->RequestAbortBattle();
}

void ABattlePreparationController::RequestAbortBattle()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance) { return; }

	UGameContextManager* ContextManager =
		GameInstance->GetSubsystem<UGameContextManager>();

	if (!ContextManager) { return; }

	UE_LOG(LogTemp, Log,
		TEXT("[BattlePreparationController] Requesting abort battle"));

	ContextManager->RequestAbortBattle();
}

void ABattlePreparationController::CreateInventoryWidget()
{
	if (!IsValid(InventoryWidgetClass) || InventoryWidget) { return; }

	InventoryWidget = CreateWidget<UInventoryWidget>(this, InventoryWidgetClass);
	if (!InventoryWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[BattlePreparationController] CreateInventoryWidget failed."));
		return;
	}

	// ZOrder 200 — renders above the preparation widget (default 0) but below
	// any modal dialogs that might be added later.
	InventoryWidget->AddToViewport(200);
	InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
	UE_LOG(LogTemp, Log, TEXT("[BattlePreparationController] InventoryWidget created."));
}

// ---------------------------------------------------------------------------
// Private — inventory input handlers
// ---------------------------------------------------------------------------

void ABattlePreparationController::OnInventory(const FInputActionValue& Value)
{
	if (!InventoryWidget) return;

	const bool bIsOpen = InventoryWidget->GetVisibility() == ESlateVisibility::Visible;

	if (bIsOpen)
	{
		InventoryWidget->CloseInventory();
		SetInputMode(FInputModeUIOnly());
		UE_LOG(LogTemp, Log, TEXT("[BattlePreparationController] Inventory closed."));
	}
	else
	{
		InventoryWidget->OpenInventory();
		// GameAndUI: cursor stays usable for mouse clicks; keyboard reaches controller for navigation.
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		UE_LOG(LogTemp, Log, TEXT("[BattlePreparationController] Inventory opened."));
	}
}

void ABattlePreparationController::OnInventoryPrevChar(const FInputActionValue& Value)
{
	if (!InventoryWidget || InventoryWidget->GetVisibility() != ESlateVisibility::Visible) return;
	InventoryWidget->SelectPreviousCharacter();
}

void ABattlePreparationController::OnInventoryNextChar(const FInputActionValue& Value)
{
	if (!InventoryWidget || InventoryWidget->GetVisibility() != ESlateVisibility::Visible) return;
	InventoryWidget->SelectNextCharacter();
}

void ABattlePreparationController::OnInventoryNavigateUp(const FInputActionValue& Value)
{
	if (!InventoryWidget || InventoryWidget->GetVisibility() != ESlateVisibility::Visible) return;
	InventoryWidget->NavigateUp();
}

void ABattlePreparationController::OnInventoryNavigateDown(const FInputActionValue& Value)
{
	if (!InventoryWidget || InventoryWidget->GetVisibility() != ESlateVisibility::Visible) return;
	InventoryWidget->NavigateDown();
}

void ABattlePreparationController::OnInventoryNavigateLeft(const FInputActionValue& Value)
{
	if (!InventoryWidget || InventoryWidget->GetVisibility() != ESlateVisibility::Visible) return;
	InventoryWidget->NavigateLeft();
}

void ABattlePreparationController::OnInventoryNavigateRight(const FInputActionValue& Value)
{
	if (!InventoryWidget || InventoryWidget->GetVisibility() != ESlateVisibility::Visible) return;
	InventoryWidget->NavigateRight();
}

void ABattlePreparationController::OnInventoryConfirm(const FInputActionValue& Value)
{
	if (!InventoryWidget || InventoryWidget->GetVisibility() != ESlateVisibility::Visible) return;
	InventoryWidget->ConfirmSelection();
}

void ABattlePreparationController::OnInventoryCancel(const FInputActionValue& Value)
{
	if (!InventoryWidget || InventoryWidget->GetVisibility() != ESlateVisibility::Visible) return;
	if (!InventoryWidget->CancelSelection())
	{
		InventoryWidget->CloseInventory();
		SetInputMode(FInputModeUIOnly());
		UE_LOG(LogTemp, Log, TEXT("[BattlePreparationController] Inventory closed via Cancel."));
	}
}

// ---------------------------------------------------------------------------
// Private — input handlers
// ---------------------------------------------------------------------------

void ABattlePreparationController::OnConfirm(const FInputActionValue& Value)
{
	SendRequestBattleStart();
}

void ABattlePreparationController::OnCancel(const FInputActionValue& Value)
{
	RequestAbortBattle();
}

void ABattlePreparationController::HandleStartBattleClicked()
{
	UE_LOG(LogTemp, Log,
		TEXT("[BattlePreparationController] Start Battle widget button — forwarding to RequestBattleStart"));

	SendRequestBattleStart();
}

void ABattlePreparationController::HandleAbortBattleClicked()
{
	UE_LOG(LogTemp, Log,
		TEXT("[BattlePreparationController] Start Battle widget button — forwarding to RequestBattleStart"));

	SendRequestAbortBattle();
}