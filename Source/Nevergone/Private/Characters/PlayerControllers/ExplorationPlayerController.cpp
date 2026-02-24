// Copyright Xyzto Works


#include "Characters/PlayerControllers/ExplorationPlayerController.h"

#include "EnhancedInputComponent.h"
#include "ActorComponents/ExplorationModeComponent.h"
#include "Characters/CharacterBase.h"

#include "Interfaces/ExplorationInputReceiver.h"

void AExplorationPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(LookInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::OnLook);
		EIC->BindAction(InteractionInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleInteract);
		EIC->BindAction(SaveGameInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleSave);
		EIC->BindAction(LoadGameInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleLoad);
	}
}

void AExplorationPlayerController::OnLook(const FInputActionValue& Value)
{
	const FVector2D LookAxis = Value.Get<FVector2D>();

	AddYawInput(LookAxis.X * MouseSensitivity);
	AddPitchInput(LookAxis.Y * MouseSensitivity);
}

void AExplorationPlayerController::HandleInteract()
{
	if (IExplorationInputReceiver* Actions = GetPawn()->FindComponentByClass<UExplorationModeComponent>())
	{
		Actions->Input_Interact();
	}
}

void AExplorationPlayerController::HandleSave()
{
	if (IExplorationInputReceiver* Actions = GetPawn()->FindComponentByClass<UExplorationModeComponent>())
	{
		Actions->Input_Save();
	}
}

void AExplorationPlayerController::HandleLoad()
{
	if (IExplorationInputReceiver* Actions = GetPawn()->FindComponentByClass<UExplorationModeComponent>())
	{
		Actions->Input_Load();
	}
}

void AExplorationPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	SetInputMode(FInputModeGameOnly());
}

void AExplorationPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (ACharacterBase* PossessedCharacter = Cast<ACharacterBase>(InPawn))
	{
		PossessedCharacter->EnableExplorationMode();
	}
}
