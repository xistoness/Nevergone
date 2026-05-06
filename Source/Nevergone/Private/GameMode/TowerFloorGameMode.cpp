// Copyright Xyzto Works


#include "GameMode/TowerFloorGameMode.h"

#include "Audio/AudioSubsystem.h"
#include "Characters/CharacterBase.h"
#include "Characters/PlayerControllers/BattlePlayerController.h"
#include "Characters/PlayerControllers/BattlePreparationController.h"
#include "Characters/PlayerControllers/BattleResultsController.h"
#include "GameInstance/GameContextManager.h"
#include "GameInstance/MyGameInstance.h"
#include "GameMode/CombatManager.h"
#include "NevergonePlayerController.h"

#include "Kismet/GameplayStatics.h"
#include "Party/PartyManagerSubsystem.h"

ATowerFloorGameMode::ATowerFloorGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATowerFloorGameMode::BeginPlay()
{
	Super::BeginPlay();
	SetupFloor();

	UMyGameInstance* MyGI = GetGameInstance<UMyGameInstance>();
	if (!MyGI)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[TowerFloorGameMode] BeginPlay: MyGameInstance is null — aborting setup."));
		return;
	}

	UGameContextManager* ContextManager = MyGI->GetSubsystem<UGameContextManager>();
	if (!ContextManager)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[TowerFloorGameMode] BeginPlay: GameContextManager subsystem is null — aborting setup."));
		return;
	}

	// ORDERING NOTE:
	// HandleSaveLoaded MUST run before RequestInitialState so that
	// ActiveBattleSession.ExplorationCharacterClass and ExplorationCharacterTransform
	// are populated before SpawnExplorationActorFromClass is called.
	//
	// Previously HandleSaveLoaded was triggered from
	// WorldManagerSubsystem::HandleWorldInitializedActors, which fires BEFORE
	// BeginPlay — meaning GetAuthGameMode() returned null there and the entire
	// function was aborting at the "if (!GM) { return; }" guard.
	// Moving the call here guarantees the GameMode exists when HandleSaveLoaded
	// reads ExplorationCharacterClass from it.
	ContextManager->HandleSaveLoaded();

	// Subscribe AFTER HandleSaveLoaded so the initial state transition fires
	// with fully populated session data.
	ContextManager->OnGameContextChanged.AddUObject(
		this,
		&ATowerFloorGameMode::HandleGameContextChanged
	);
	UE_LOG(LogTemp, Warning, TEXT("Bind -> Manager: %p"), ContextManager);
	UE_LOG(LogTemp, Error,
		TEXT("[TowerFloorGameMode] BeginPlay: subscribed to OnGameContextChanged. this=%s | IsBound=%d"),
		*GetNameSafe(this), ContextManager->OnGameContextChanged.IsBound() ? 1 : 0);

	// Tell the GameContextManager to enter the floor's initial state.
	// This fires OnGameContextChanged, which HandleGameContextChanged picks up —
	// music, controller swap, pawn spawn, and entry point application run from there.
	ContextManager->RequestInitialState(InitialContextState);

	UPartyManagerSubsystem* Party = MyGI->GetSubsystem<UPartyManagerSubsystem>();
	if (Party && Party->GetPartyData().Members.Num() == 0)
	{
		// No party loaded from save — populate debug party for testing.
		// This only runs when the party is genuinely empty (new game, no save data).
		for (int32 i = 0; i < TestCharClass.Num(); ++i)
		{
			if (!TestCharClass[i]) { continue; }
			FPartyMemberData DebugMember;
			DebugMember.CharacterClass = TestCharClass[i];
			DebugMember.Level          = 1 + i;
			DebugMember.bIsAlive       = true;
			DebugMember.CurrentHP      = 0.f; // 0 = use MaxHP on first battle init
			DebugMember.CharacterID    = FGuid::NewGuid();
			Party->AddPartyMember(DebugMember);
		}
		// Push to GameInstance so the save system sees the debug party.
		Party->FlushToGameInstance();
	}
}

void ATowerFloorGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogTemp, Error, TEXT("GameMode EndPlay called!"));
	if (UMyGameInstance* MyGI = GetGameInstance<UMyGameInstance>())
	{
		if (UGameContextManager* ContextManager = MyGI->GetSubsystem<UGameContextManager>())
		{
			UE_LOG(LogTemp, Log,
				TEXT("[TowerFloorGameMode] EndPlay: unsubscribing from OnGameContextChanged. Reason=%d"),
				(int32)EndPlayReason);
			ContextManager->OnGameContextChanged.RemoveAll(this);
		}
	}

	TeardownFloor();
	Super::EndPlay(EndPlayReason);
}

void ATowerFloorGameMode::SetupFloor()
{
	// Spawn or enable level-wide systems
}

void ATowerFloorGameMode::TeardownFloor()
{
	// Cleanup level-wide systems
}

APawn* ATowerFloorGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// Returning nullptr prevents AGameModeBase::SpawnDefaultPawnFor from
	// spawning anything. GameContextManager is responsible for spawning
	// and possessing the exploration pawn.
	return nullptr;
}

void ATowerFloorGameMode::HandleGameContextChanged(EGameContextState NewState)
{
    UMyGameInstance* MyGI = GetGameInstance<UMyGameInstance>();
    if (!MyGI) { return; }

	UE_LOG(LogTemp, Warning, TEXT("GameMode %p received state %d"), this, (int)NewState);
	
    UAudioSubsystem* Audio = MyGI->GetSubsystem<UAudioSubsystem>();
    UGameContextManager* ContextManager = MyGI->GetSubsystem<UGameContextManager>();

    APlayerController* NewPC = nullptr;

    switch (NewState)
    {
        case EGameContextState::Exploration:
        {
            if (!ExplorationControllerClass)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("[TowerFloorGameMode] Exploration: ExplorationControllerClass is null."));
                break;
            }

            if (!ActivePlayerController)
            {
                ActivePlayerController = UGameplayStatics::GetPlayerController(this, 0);
            }

            APlayerController* OldPC = ActivePlayerController;

            if (OldPC && OldPC->IsA(ExplorationControllerClass))
            {
                NewPC = OldPC;
            }
            else
            {
                NewPC = GetWorld()->SpawnActor<APlayerController>(ExplorationControllerClass);
                if (!NewPC)
                {
                    UE_LOG(LogTemp, Error,
                        TEXT("[TowerFloorGameMode] Exploration: failed to spawn controller."));
                    break;
                }

                SwapPlayerControllers(OldPC, NewPC);
                ActivePlayerController = NewPC;

                if (OldPC && OldPC->IsA<ANevergonePlayerController>())
                {
                    OldPC->Destroy();
                }
            }

            // Play exploration music unconditionally — covers both the initial load
            // and the return from combat (where OldPC may already be the exploration
            // controller, causing the old code to skip PlayMusic entirely).
            if (Audio)
            {
                Audio->PlayMusic(ExplorationMusic, EMusicState::Exploration);
            }

            if (!ContextManager) { break; }

            if (APawn* OldPawn = NewPC->GetPawn())
            {
                NewPC->UnPossess();
                OldPawn->Destroy();
            }

            ACharacterBase* ExplorationCharacter =
                ContextManager->GetSavedExplorationCharacter();

            if (!ExplorationCharacter)
            {
                TSubclassOf<ACharacterBase> PawnClass =
                    ContextManager->ResolveExplorationPawnClass(ExplorationCharacterClass);

                ExplorationCharacter =
                    ContextManager->SpawnExplorationActorFromClass(PawnClass);
            }

            if (ExplorationCharacter)
            {
                NewPC->Possess(ExplorationCharacter);

                if (NewPC->GetPawn() == ExplorationCharacter)
                {
                    ContextManager->ClearBattleSession();
                    MyGI->ApplyPendingEntryPoint(GetWorld());
                }
            }

            break;
        }

        case EGameContextState::BattlePreparation:
        {
            NewPC = SpawnAndSwapPlayerController(BattlePreparationControllerClass);
            if (!NewPC)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("[TowerFloorGameMode] BattlePreparation: failed to spawn controller."));
                break;
            }

            if (ContextManager)
            {
                if (ABattlePreparationController* PrepPC =
                    Cast<ABattlePreparationController>(NewPC))
                {
                    PrepPC->SetPreparationContext(
                        ContextManager->GetActivePrepContext());
                }
            }

            break;
        }

        case EGameContextState::Battle:
        {
            if (Audio)
            {
                Audio->PlayMusic(BattleMusic, EMusicState::Battle);
            }

            NewPC = SpawnAndSwapPlayerController(BattleControllerClass);

            if (NewPC)
            {
                if (ABattlePlayerController* BattlePC =
                    Cast<ABattlePlayerController>(NewPC))
                {
                    if (ContextManager)
                    {
                        if (UCombatManager* CM =
                            ContextManager->GetActiveCombatManager())
                        {
                            if (!BattlePC->HasBattleCamera())
                            {
                                BattlePC->EnterBattleMode(CM);
                            }
                        }
                    }
                }
            }

            break;
        }

        case EGameContextState::BattleResults:
        {
            NewPC = SpawnAndSwapPlayerController(BattleResultsControllerClass);
            if (!NewPC) { break; }

            if (ContextManager)
            {
                if (ABattleResultsController* ResultsPC =
                    Cast<ABattleResultsController>(NewPC))
                {
                    ResultsPC->SetResultsContext(
                        ContextManager->GetActiveResultsContext());
                }
            }

            break;
        }

        default:
            break;
    }
}

APlayerController* ATowerFloorGameMode::SpawnAndSwapPlayerController(
	TSubclassOf<APlayerController> NewControllerClass)
{
	if (!NewControllerClass)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[TowerFloorGameMode] SpawnAndSwapPlayerController: NewControllerClass is null. "
			     "Make sure all controller classes are assigned in BP_TowerFloorGameMode CDO "
			     "(ExplorationControllerClass, BattleControllerClass, BattlePreparationControllerClass, BattleResultsControllerClass)."));
		return nullptr;
	}

	if (!ActivePlayerController)
	{
		ActivePlayerController = UGameplayStatics::GetPlayerController(this, 0);
	}

	if (!ActivePlayerController)
	{
		return nullptr;
	}

	if (ActivePlayerController->IsA(NewControllerClass))
	{
		return ActivePlayerController;
	}

	APlayerController* OldPC = ActivePlayerController;

	APlayerController* NewPC = GetWorld()->SpawnActor<APlayerController>(NewControllerClass);
	if (!NewPC)
	{
		return nullptr;
	}

	SwapPlayerControllers(OldPC, NewPC);
	ActivePlayerController = NewPC;

	// Only destroy controllers we created. The engine-created PIE controller must
	// not be destroyed — doing so invalidates the PIE GameMode in the GC.
	if (OldPC->IsA<ANevergonePlayerController>())
	{
		UE_LOG(LogTemp, Log,
			TEXT("[TowerFloorGameMode] SpawnAndSwap: destroying OldPC %s (our controller type)."),
			*GetNameSafe(OldPC));
		OldPC->Destroy();
	}
	else
	{
		UE_LOG(LogTemp, Log,
			TEXT("[TowerFloorGameMode] SpawnAndSwap: skipping Destroy for engine-created OldPC %s."),
			*GetNameSafe(OldPC));
	}

	return NewPC;
}

void ATowerFloorGameMode::SwitchToExplorationController()
{
	APlayerController* NewPC = SpawnAndSwapPlayerController(ExplorationControllerClass);
	if (!NewPC) { return; }

	if (UMyGameInstance* MyGI = GetGameInstance<UMyGameInstance>())
	{
		if (UGameContextManager* ContextManager = MyGI->GetSubsystem<UGameContextManager>())
		{
			ACharacterBase* ExplorationCharacter = ContextManager->GetSavedExplorationCharacter();
			if (ExplorationCharacter)
			{
				NewPC->Possess(ExplorationCharacter);
				ContextManager->ClearBattleSession();
			}
		}
	}
}

void ATowerFloorGameMode::SwitchToBattlePreparationController()
{
	APlayerController* NewPC = SpawnAndSwapPlayerController(BattlePreparationControllerClass);
	if (!NewPC) { return; }

	APawn* ExistingPawn = nullptr;
	if (APlayerController* OldPC = UGameplayStatics::GetPlayerController(this, 0))
	{
		ExistingPawn = OldPC->GetPawn();
	}

	if (ExistingPawn && !NewPC->GetPawn())
	{
		NewPC->Possess(ExistingPawn);
	}
}

void ATowerFloorGameMode::SwitchToBattleController()
{
	APlayerController* NewPC = SpawnAndSwapPlayerController(BattleControllerClass);
	if (!NewPC) { return; }

	APawn* ExistingPawn = nullptr;
	if (APlayerController* OldPC = UGameplayStatics::GetPlayerController(this, 0))
	{
		ExistingPawn = OldPC->GetPawn();
	}

	if (ExistingPawn && !NewPC->GetPawn())
	{
		NewPC->Possess(ExistingPawn);
	}
}