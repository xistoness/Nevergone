// Copyright Xyzto Works


#include "GameMode/TowerFloorGameMode.h"

#include "Audio/AudioSubsystem.h"
#include "Characters/CharacterBase.h"
#include "GameInstance/GameContextManager.h"

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
 
	// Debug Party feeding
	UPartyManagerSubsystem* Party = GetGameInstance()->GetSubsystem<UPartyManagerSubsystem>();
	Party->ClearParty();
	for (int i = 0; i < 4; ++i)
	{
		FPartyMemberData DebugMember;
		DebugMember.CharacterClass = TestCharClass[i];
		DebugMember.Level = 1 + i;
		DebugMember.bIsAlive = true;
		Party->AddPartyMember(DebugMember);
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
		SwitchPlayerController(BattlePreparationControllerClass);
		break;

	case EGameContextState::Battle:
		if (Audio)
		{
			Audio->PlayMusic(BattleMusic, EMusicState::Battle);
		}
		SwitchPlayerController(BattleControllerClass);
		break;

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

void ATowerFloorGameMode::SwitchPlayerController(
	TSubclassOf<APlayerController> NewControllerClass)
{
	if (!NewControllerClass)
	{
		return;
	}

	if (!ActivePlayerController)
	{
		ActivePlayerController = UGameplayStatics::GetPlayerController(this, 0);
	}

	if (!ActivePlayerController || ActivePlayerController->IsA(NewControllerClass))
	{
		return;
	}

	APlayerController* OldPC = ActivePlayerController;
	APawn* Pawn = OldPC->GetPawn();

	if (Pawn)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[TowerFloorGameMode]: Unpossessing pawn before controller swap: %s"),
			*GetNameSafe(Pawn));

		OldPC->UnPossess();
	}

	APlayerController* NewPC = GetWorld()->SpawnActor<APlayerController>(NewControllerClass);
	if (!NewPC)
	{
		return;
	}

	SwapPlayerControllers(OldPC, NewPC);

	if (Pawn && IsValid(Pawn))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[TowerFloorGameMode]: Repossessing pawn after controller swap: %s"),
			*GetNameSafe(Pawn));

		NewPC->Possess(Pawn);
	}
	else
	{
		UE_LOG(LogTemp, Error,
			TEXT("[TowerFloorGameMode]: Pawn became invalid during controller swap."));
	}

	ActivePlayerController = NewPC;
}
