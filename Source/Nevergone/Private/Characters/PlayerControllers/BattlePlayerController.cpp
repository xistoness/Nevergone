// Copyright Xyzto Works

#include "Characters/PlayerControllers/BattlePlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Characters/CharacterBase.h"
#include "GameMode/CombatManager.h"
#include "GameMode/Combat/BattleInputManager.h"
#include "GameMode/Combat/BattleCameraPawn.h"
#include "Level/GridManager.h"
#include "Widgets/Combat/ActionHotbar.h"
#include "Widgets/Combat/BattleHUDWidget.h"

void ABattlePlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void ABattlePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	
	HandleHover();
}

void ABattlePlayerController::SpawnPreviewActors()
{
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Spawning Preview Actor"));
		PreviewActor = GetWorld()->SpawnActor<AActor>(
			PreviewActorClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator
		);

		PreviewActor->SetActorHiddenInGame(true);
	}

	if (PreviewBlockedActorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Spawning Preview Blocked Actor"));
		PreviewBlockedActor = GetWorld()->SpawnActor<AActor>(
			PreviewBlockedActorClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator
		);

		PreviewBlockedActor->SetActorHiddenInGame(true);
	}
}

void ABattlePlayerController::EnterBattleMode(UCombatManager* InCombatManager)
{
	UE_LOG(LogTemp, Log, TEXT("[BattlePlayerController] Asked to EnterBattleMode"));
	CombatManager = InCombatManager;
	
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	SetInputMode(FInputModeGameAndUI());
	
	BattleInputManager = CombatManager
		? CombatManager->GetBattleInputManager()
		: nullptr;

	SpawnAndPossessBattleCamera();
	CombatManager->RegisterBattleCamera(BattleCameraPawn);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->ClearAllMappings();

			if (BattleInputContext)
			{
				Subsystem->AddMappingContext(BattleInputContext, 0);

				UE_LOG(LogTemp, Log,
					TEXT("[BattlePlayerController] Battle input context applied"));
			}
		}
	}
	
	SpawnPreviewActors();
	CreateAndInitializeHotbar();
	CreateAndInitializeHUD();
}

void ABattlePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput =
		Cast<UEnhancedInputComponent>(InputComponent);

	if (!EnhancedInput)
	{
		return;
	}

#define BIND(Action, Event, Func) \
	if (Action) { EnhancedInput->BindAction(Action, Event, this, Func); }

	BIND(ConfirmAction,        ETriggerEvent::Triggered, &ABattlePlayerController::HandleConfirm);
	BIND(CancelAction,         ETriggerEvent::Triggered, &ABattlePlayerController::HandleCancel);
	BIND(SelectNextUnit,     ETriggerEvent::Triggered, &ABattlePlayerController::HandleSelectNextUnit);
	BIND(SelectPreviousUnit, ETriggerEvent::Triggered, &ABattlePlayerController::HandleSelectPreviousUnit);
	BIND(SelectNextAction,     ETriggerEvent::Triggered, &ABattlePlayerController::HandleSelectNextAction);
	BIND(SelectPreviousAction, ETriggerEvent::Triggered, &ABattlePlayerController::HandleSelectPreviousAction);
	BIND(EndTurnAction,        ETriggerEvent::Triggered, &ABattlePlayerController::HandleEndTurn);
	BIND(CameraMoveAction,     ETriggerEvent::Triggered, &ABattlePlayerController::HandleCameraMove);
	BIND(CameraZoomAction,     ETriggerEvent::Triggered, &ABattlePlayerController::HandleCameraZoom);
	BIND(CameraRotateAction,   ETriggerEvent::Triggered, &ABattlePlayerController::HandleCameraRotate);

#undef BIND
}

void ABattlePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (ACharacterBase* PossessedCharacter = Cast<ACharacterBase>(InPawn))
	{
		PossessedCharacter->EnableBattleMode();
	}
}

void ABattlePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
	if (PreviewBlockedActor)
	{
		PreviewBlockedActor->Destroy();
		PreviewBlockedActor = nullptr;
	}
	DestroyHotbar();
	DestroyHUD();
}

/* ---------- Input forwarding ---------- */

void ABattlePlayerController::HandleConfirm()
{
	if (!BattleInputManager)
	{
		return;
	}
	
	if (BattleInputManager->CanAcceptInput())
	{
		BattleInputManager->OnConfirmPressed();
	}
}

void ABattlePlayerController::HandleCancel()
{
	if (!BattleInputManager)
	{
		return;
	}
	
	if (BattleInputManager->CanAcceptInput())
	{
		BattleInputManager->OnCancelPressed();
	}	
}

void ABattlePlayerController::HandleHover()
{
	if (!BattleInputManager)
	{
		return;
	}
	
	if (!BattleInputManager->CanAcceptInput())
	{
		return;
	}

	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_GameTraceChannel2, false, Hit))
	{
		UGridManager* GridManager = GetWorld()->GetSubsystem<UGridManager>();
		if (!GridManager)
		{
			return;
		}
		
		BattleInputManager->OnHover(Hit.Location);
	}
}

void ABattlePlayerController::HandleSelectNextUnit()
{
	UE_LOG(LogTemp, Warning, TEXT("[BattlePlayerController] Inputs select next unit..."));
	if (!BattleInputManager)
	{
		return;
	}	
	
	if (BattleInputManager->CanAcceptInput())
	{
		BattleInputManager->OnSelectNextUnit();
	}
}

void ABattlePlayerController::HandleSelectPreviousUnit()
{
	UE_LOG(LogTemp, Warning, TEXT("[BattlePlayerController] Inputs select previous unit..."));
	if (!BattleInputManager)
	{
		return;
	}	
	
	if (BattleInputManager->CanAcceptInput())
	{
		BattleInputManager->OnSelectPreviousUnit();
	}
}

void ABattlePlayerController::HandleSelectNextAction()
{
	UE_LOG(LogTemp, Warning, TEXT("[BattlePlayerController] Inputs select next action..."));
	if (!BattleInputManager)
	{
		return;
	}	
	if (BattleInputManager->CanAcceptInput())
	{
		BattleInputManager->OnSelectNextAction();
	}
}

void ABattlePlayerController::HandleSelectPreviousAction()
{
	UE_LOG(LogTemp, Warning, TEXT("[BattlePlayerController] Inputs select previous action..."));
	if (!BattleInputManager)
	{
		return;
	}	
	if (BattleInputManager->CanAcceptInput())
	{
		BattleInputManager->OnSelectPreviousAction();
	}
}

void ABattlePlayerController::HandleEndTurn()
{
	if (!BattleInputManager)
	{
		return;
	}	
	if (BattleInputManager->CanAcceptInput())
	{
		BattleInputManager->OnEndTurn();
	}
}

void ABattlePlayerController::HandleCameraMove(const FInputActionValue& Value)
{
	if (!BattleInputManager)
	{
		return;
	}	
	if (BattleInputManager->CanCameraAcceptInput())
	{
		BattleInputManager->OnCameraMove(Value.Get<FVector2D>());
	}
}

void ABattlePlayerController::HandleCameraZoom(const FInputActionValue& Value)
{
	if (!BattleInputManager)
	{
		return;
	}	
	if (BattleInputManager->CanCameraAcceptInput())
	{
		BattleInputManager->OnCameraZoom(Value.Get<float>());
	}
}

void ABattlePlayerController::HandleCameraRotate(const FInputActionValue& Value)
{
	if (!BattleInputManager)
	{
		return;
	}	
	if (BattleInputManager->CanCameraAcceptInput())
	{
		BattleInputManager->OnCameraRotate(Value.Get<float>());
	}
}

/* ---------- Camera ---------- */

void ABattlePlayerController::SpawnAndPossessBattleCamera()
{
	UE_LOG(LogTemp, Log, TEXT("[BattlePlayerController] Trying to spawn and possess battle camera..."));
	if (!BattleCameraPawnClass || BattleCameraPawn)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	BattleCameraPawn = World->SpawnActor<ABattleCameraPawn>(
		BattleCameraPawnClass,
		FVector::ZeroVector,
		FRotator(-60.f, 0.f, 0.f),
		Params
	);

	if (BattleCameraPawn)
	{
		Possess(BattleCameraPawn);
		UE_LOG(LogTemp, Log, TEXT("[BattlePlayerController] Possessed Battle Camera Pawn!"));
	}
}

void ABattlePlayerController::CreateAndInitializeHotbar()
{
	if (!ActionHotbarClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BattlePlayerController] CreateAndInitializeHotbar: ActionHotbarClass is not set — "
				 "assign WBP_ActionHotbar to the CombatManager Blueprint subclass."));
		return;
	}
	
	if (ActionHotbar) { return;}
 
	// CreateWidget requires a valid PlayerController as owner so UMG can
	// attach the widget to the correct local player and manage its lifetime.
	ActionHotbar = CreateWidget<UActionHotbar>(this, ActionHotbarClass);
	if (!ActionHotbar)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[BattlePlayerController] CreateAndInitializeHotbar: CreateWidget failed."));
		return;
	}
	
	if (!CombatManager) {return; }
 
	ActionHotbar->AddToViewport();
	ActionHotbar->InitializeWithCombatManager(CombatManager);
 
	UE_LOG(LogTemp, Log,
		TEXT("[BattlePlayerController] CreateAndInitializeHotbar: ActionHotbar created and bound."));
}
 
void ABattlePlayerController::DestroyHotbar()
{
	if (!ActionHotbar)
	{
		return;
	}
 
	// ClearHotbar unbinds all delegates before the widget is removed,
	// preventing callbacks from firing on a partially torn-down object.
	ActionHotbar->ClearHotbar();
	ActionHotbar->RemoveFromParent();
	ActionHotbar = nullptr;
 
	UE_LOG(LogTemp, Log, TEXT("[CombatManager] DestroyHotbar: ActionHotbar removed."));
}
void ABattlePlayerController::CreateAndInitializeHUD()
{
	if (!BattleHUDClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BattlePlayerController] CreateAndInitializeHUD: BattleHUDClass not set — assign WBP_BattleHUD in the Blueprint subclass."));
		return;
	}

	if (!CombatManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[BattlePlayerController] CreateAndInitializeHUD: CombatManager is null"));
		return;
	}

	BattleHUD = CreateWidget<UBattleHUDWidget>(this, BattleHUDClass);
	if (!BattleHUD)
	{
		UE_LOG(LogTemp, Error, TEXT("[BattlePlayerController] CreateAndInitializeHUD: CreateWidget failed"));
		return;
	}

	// Add to viewport before initializing so NativeTick is active when HP bars are placed
	BattleHUD->AddToViewport(5);
	BattleHUD->InitializeWithCombatManager(CombatManager);

	UE_LOG(LogTemp, Log, TEXT("[BattlePlayerController] BattleHUD created and initialized."));
}

void ABattlePlayerController::DestroyHUD()
{
	if (!BattleHUD)
	{
		return;
	}

	BattleHUD->Deinitialize();
	BattleHUD->RemoveFromParent();
	BattleHUD = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[BattlePlayerController] BattleHUD destroyed."));
}