// Copyright Xyzto Works


#include "Characters/PlayerControllers/ExplorationPlayerController.h"

#include "EnhancedInputComponent.h"
#include "ActorComponents/ExplorationModeComponent.h"
#include "Characters/CharacterBase.h"
#include "GameInstance/GameContextManager.h"

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

void AExplorationPlayerController::ApplyExplorationInputMode()
{
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}

void AExplorationPlayerController::OnLook(const FInputActionValue& Value)
{
	const FVector2D LookAxis = Value.Get<FVector2D>();

	AddYawInput(LookAxis.X * MouseSensitivity);
	AddPitchInput(LookAxis.Y * MouseSensitivity);
}

void AExplorationPlayerController::HandleInteract()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn) { return; }

	if (IExplorationInputReceiver* Actions = MyPawn->FindComponentByClass<UExplorationModeComponent>())
	{
		Actions->Input_Interact();
	}
}

void AExplorationPlayerController::HandleSave()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn) { return; }

	if (IExplorationInputReceiver* Actions = MyPawn->FindComponentByClass<UExplorationModeComponent>())
	{
		Actions->Input_Save();
	}
}

void AExplorationPlayerController::HandleLoad()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn) { return; }

	if (IExplorationInputReceiver* Actions = MyPawn->FindComponentByClass<UExplorationModeComponent>())
	{
		Actions->Input_Load();
	}
}

void AExplorationPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ApplyExplorationInputMode();
}

void AExplorationPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UE_LOG(LogTemp, Warning, TEXT("[ExplorationPC]: OnPossess -> %s"), *GetNameSafe(InPawn));
	
	ApplyDefaultInputMappings();
	
	if (ACharacterBase* PossessedCharacter = Cast<ACharacterBase>(InPawn))
	{
		PossessedCharacter->ApplyCharacterInputMapping();
		PossessedCharacter->EnableExplorationMode();
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UGameContextManager* ContextManager = GI->GetSubsystem<UGameContextManager>())
		{
			SetControlRotation(ContextManager->GetSavedExplorationControlRotation());
		}
	}
	
	ApplyExplorationInputMode();
	
}

void AExplorationPlayerController::OnUnPossess()
{
	UE_LOG(LogTemp, Warning, TEXT("[ExplorationPC]: OnUnPossess -> %s"), *GetNameSafe(GetPawn()));
	Super::OnUnPossess();
}