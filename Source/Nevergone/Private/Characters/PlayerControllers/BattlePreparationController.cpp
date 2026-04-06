// Copyright Xyzto Works

#include "Characters/PlayerControllers/BattlePreparationController.h"

#include "EnhancedInputComponent.h"
#include "GameInstance/GameContextManager.h"
#include "GameMode/BattlePreparationContext.h"
#include "Widgets/Combat/BattlePreparationWidget.h"

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
	}
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ABattlePreparationController::BeginPlay()
{
	Super::BeginPlay();

	// Show cursor — the preparation screen is fully UI-driven.
	bShowMouseCursor = true;
	SetInputMode(FInputModeUIOnly());
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