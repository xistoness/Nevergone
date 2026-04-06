// Copyright Xyzto Works


#include "GameMode/TowerFloorGameMode.h"

#include "Audio/AudioSubsystem.h"
#include "Characters/CharacterBase.h"
#include "Characters/PlayerControllers/BattlePlayerController.h"
#include "Characters/PlayerControllers/BattlePreparationController.h"
#include "Characters/PlayerControllers/BattleResultsController.h"
#include "GameInstance/GameContextManager.h"
#include "GameMode/CombatManager.h"

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
 
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UGameContextManager* ContextManager = GI->GetSubsystem<UGameContextManager>())
		{
			// Subscribe first, so the initial state transition fires into our handler
			ContextManager->OnGameContextChanged.AddUObject(
				this,
				&ATowerFloorGameMode::HandleGameContextChanged
			);
 
			// Tell the GameContextManager to enter the floor's initial state.
			// This fires OnGameContextChanged, which HandleGameContextChanged picks up —
			// music, controller swap, and any other context logic runs from there.
			ContextManager->RequestInitialState(InitialContextState);
		}
	}
	UPartyManagerSubsystem* Party = GetGameInstance()->GetSubsystem<UPartyManagerSubsystem>();
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
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UGameContextManager* ContextManager =
			GI->GetSubsystem<UGameContextManager>())
		{
			ContextManager->OnGameContextChanged.RemoveAll(this);
		}
	}
	
	TeardownFloor();
	Super::EndPlay(EndPlayReason);
}

void ATowerFloorGameMode::SetupFloor()
{
	// Spawn or enable level-wide systems
	// Example: WorldManagerSubsystem initialization
}

void ATowerFloorGameMode::TeardownFloor()
{
	// Cleanup level-wide systems
}

void ATowerFloorGameMode::HandleGameContextChanged(EGameContextState NewState)
{
	UAudioSubsystem* Audio = nullptr;
	UGameInstance* GI = GetGameInstance();
	if (GI)
	{
		Audio = GI->GetSubsystem<UAudioSubsystem>();
	}
	
	switch (NewState)
	{
	case EGameContextState::Exploration:
		{
			if (!ExplorationControllerClass)
			{
				return;
			}

			if (!ActivePlayerController)
			{
				ActivePlayerController = UGameplayStatics::GetPlayerController(this, 0);
			}

			APlayerController* OldPC = ActivePlayerController;
			APlayerController* NewPC = nullptr;

			if (OldPC && OldPC->IsA(ExplorationControllerClass))
			{
				NewPC = OldPC;
			}
			else
			{
				NewPC = GetWorld()->SpawnActor<APlayerController>(ExplorationControllerClass);
				if (!NewPC)
				{
					return;
				}

				SwapPlayerControllers(OldPC, NewPC);
				ActivePlayerController = NewPC;
			}
			
			if (Audio)
			{
				Audio->PlayMusic(ExplorationMusic, EMusicState::Exploration);
			}

			if (GI)
			{
				if (UGameContextManager* ContextManager = GI->GetSubsystem<UGameContextManager>())
				{
					ACharacterBase* ExplorationCharacter = ContextManager->GetSavedExplorationCharacter();
					if (ExplorationCharacter)
					{
						UE_LOG(LogTemp, Warning,
							TEXT("[TowerFloorGameMode]: Possessing saved exploration character: %s"),
							*GetNameSafe(ExplorationCharacter));

						NewPC->Possess(ExplorationCharacter);

						if (NewPC->GetPawn() == ExplorationCharacter)
						{
							UE_LOG(LogTemp, Warning, TEXT("[TowerFloorGameMode]: Exploration character possessed successfully."));
							ContextManager->ClearBattleSession();
						}
						else
						{
							UE_LOG(LogTemp, Error, TEXT("[TowerFloorGameMode]: Failed to possess saved exploration character."));
						}
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("[TowerFloorGameMode]: Saved exploration character is null."));
					}
				}
			}
			break;
		}

	case EGameContextState::BattlePreparation:
		{
			APlayerController* NewPC = SpawnAndSwapPlayerController(BattlePreparationControllerClass); 
			if (NewPC)
			{
				if (GI)
				{
					if (UGameContextManager* ContextManager = GI->GetSubsystem<UGameContextManager>())
					{
						if (ABattlePreparationController* PrepPC =
							Cast<ABattlePreparationController>(NewPC))
						{
							PrepPC->SetPreparationContext(
								ContextManager->GetActivePrepContext());
						}
					}
				}
			}
			break;
		}

	case EGameContextState::Battle:
		if (Audio)
		{
			Audio->PlayMusic(BattleMusic, EMusicState::Battle);
		}
		
		{
			APlayerController* NewPC = SpawnAndSwapPlayerController(BattleControllerClass);

			// If this is a mid-combat restore, the CombatManager already exists —
			// call EnterBattleMode on the freshly spawned BattlePlayerController
			// so it spawns and possesses BattleCameraPawn correctly.
			// In a normal battle flow, EnterBattleMode is called by StartCombat
			// before the controller swap — here we need it after.
			if (NewPC)
			{
				if (ABattlePlayerController* BattlePC = Cast<ABattlePlayerController>(NewPC))
				{
					if (GI)
					{
						if (UGameContextManager* CtxMgr = GI->GetSubsystem<UGameContextManager>())
						{
							if (UCombatManager* CM = CtxMgr->GetActiveCombatManager())
							{
								// Only call if BattleCameraPawn hasn't been set yet —
								// avoids double-calling in the normal (non-restore) path
								// where EnterBattleMode was already called by StartCombat.
								if (!BattlePC->HasBattleCamera())
								{
									BattlePC->EnterBattleMode(CM);
								}
							}
						}
					}
				}
			}
		}
		break;
	
	case EGameContextState::BattleResults:
		{
			if (!BattleResultsControllerClass)
			{
				UE_LOG(LogTemp, Error,
					TEXT("[TowerFloorGameMode] BattleResultsControllerClass not set — assign in BP CDO"));
				break;
			}

			// Spawn the results controller and hand it the context so it can
			// build the widget. No pawn is possessed during this state.
			APlayerController* NewPC = SpawnAndSwapPlayerController(BattleResultsControllerClass);
			if (!NewPC) { break; }

			if (GI)
			{
				if (UGameContextManager* ContextManager = GI->GetSubsystem<UGameContextManager>())
				{
					if (ABattleResultsController* ResultsPC =
						Cast<ABattleResultsController>(NewPC))
					{
						ResultsPC->SetResultsContext(ContextManager->GetActiveResultsContext());
					}
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

	return NewPC;
}

void ATowerFloorGameMode::SwitchToExplorationController()
{
	APlayerController* NewPC = SpawnAndSwapPlayerController(ExplorationControllerClass);
	if (!NewPC)
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UGameContextManager* ContextManager = GI->GetSubsystem<UGameContextManager>())
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
	if (!NewPC)
	{
		return;
	}

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
	if (!NewPC)
	{
		return;
	}

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