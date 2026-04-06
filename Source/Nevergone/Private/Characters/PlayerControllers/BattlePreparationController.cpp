// Copyright Xyzto Works


#include "Characters/PlayerControllers/BattlePreparationController.h"

#include "EnhancedInputComponent.h"
#include "GameInstance/GameContextManager.h"

void ABattlePreparationController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(ConfirmInput, ETriggerEvent::Triggered, this, &ABattlePreparationController::OnConfirm);
		EIC->BindAction(CancelInput, ETriggerEvent::Triggered, this, &ABattlePreparationController::OnCancel);
	}
}

void ABattlePreparationController::SetPreparationContext(UBattlePreparationContext* InContext)
{
	if (!InContext) { return; }
	PreparationContext = InContext;

	// Future: spawn preparation widget here and populate from context.
	// For now, auto-confirm immediately (debug behavior preserved).
	UE_LOG(LogTemp, Log,
		TEXT("[BattlePreparationController] PreparationContext received — auto-confirming for now"));
	SendRequestBattleStart();
}

void ABattlePreparationController::BeginPlay()
{
	Super::BeginPlay();
	SetInputMode(FInputModeGameAndUI());
	bShowMouseCursor = true;

}

void ABattlePreparationController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

void ABattlePreparationController::SendRequestBattleStart()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	UGameContextManager* ContextManager =
		GameInstance->GetSubsystem<UGameContextManager>();

	if (!ContextManager)
	{
		return;
	}

	ContextManager->RequestBattleStart();
}

void ABattlePreparationController::RequestAbortBattle()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	UGameContextManager* ContextManager =
		GameInstance->GetSubsystem<UGameContextManager>();

	if (!ContextManager)
	{
		return;
	}

	ContextManager->AbortBattle();
}

void ABattlePreparationController::OnConfirm(const FInputActionValue& Value)
{
	SendRequestBattleStart();
}

void ABattlePreparationController::OnCancel(const FInputActionValue& Value)
{
	RequestAbortBattle();
}


