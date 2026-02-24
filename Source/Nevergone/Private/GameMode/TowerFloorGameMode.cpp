// Copyright Xyzto Works


#include "GameMode/TowerFloorGameMode.h"

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
		if (UGameContextManager* ContextManager =
			GI->GetSubsystem<UGameContextManager>())
		{
			ContextManager->OnGameContextChanged.AddUObject(
				this,
				&ATowerFloorGameMode::HandleGameContextChanged
			);

			// Ensure correct controller on level start
			HandleGameContextChanged(ContextManager->GetCurrentState());
		}
	}
		
	// Debug Party feeding
	UPartyManagerSubsystem* Party =
	GetGameInstance()->GetSubsystem<UPartyManagerSubsystem>();

	Party->ClearParty();

	for (int i = 0; i < 4; ++i)
	{
		FPartyMemberData DebugMember;
		DebugMember.CharacterClass = TestCharClass;
		DebugMember.Level = 10 + i;
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
	switch (NewState)
	{
	case EGameContextState::Exploration:
		SwitchPlayerController(ExplorationControllerClass);
		break;

	case EGameContextState::BattlePreparation:
		SwitchPlayerController(BattlePreparationControllerClass);
		break;
		
	case EGameContextState::Battle:
		SwitchPlayerController(BattleControllerClass);
		break;

	default:
		break;
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

	APlayerController* NewPC =
		GetWorld()->SpawnActor<APlayerController>(NewControllerClass);

	if (!NewPC)
	{
		return;
	}

	SwapPlayerControllers(OldPC, NewPC);

	if (Pawn)
	{
		NewPC->Possess(Pawn);
	}

	ActivePlayerController = NewPC;
}
