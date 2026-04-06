// Copyright Xyzto Works


#include "GameInstance/GameContextManager.h"

#include "Characters/CharacterBase.h"
#include "GameMode/TowerFloorGameMode.h"
#include "GameMode/CombatManager.h"
#include "GameMode/BattlePreparationContext.h"
#include "GameMode/BattlefieldLayoutSubsystem.h"
#include "GameMode/BattlefieldQuerySubsystem.h"
#include "GameMode/BattleResultsContext.h"
#include "Level/EncounterGeneratorSubsystem.h"
#include "Level/FloorEncounterVolume.h"
#include "Level/GridManager.h"
#include "Party/PartyManagerSubsystem.h"
#include "Types/LevelTypes.h"

#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameMode/BattleResultsContext.h"
#include "Kismet/GameplayStatics.h"


void UGameContextManager::RequestInitialState(EGameContextState InitialState)
{
	UE_LOG(LogTemp, Log, TEXT("[GameContextManager] Entering initial state: %d"), (int32)InitialState);
 
	// Bypass CanEnterState — at BeginPlay the world is not fully set up yet,
	// and this is an explicit instruction from the floor's GameMode, not a
	// runtime request that needs validation.
	EnterState(InitialState);
}

void UGameContextManager::RequestExploration()
{
	if (!CanEnterState(EGameContextState::Exploration))
		return;

	EnterState(EGameContextState::Exploration);
}

void UGameContextManager::RequestBattlePreparation(
	AFloorEncounterVolume* EncounterSource)
{
	UE_LOG(LogTemp, Warning, TEXT("[GameContextManager]: Requesting battle preparation..."));

	if (!CanEnterState(EGameContextState::BattlePreparation))
		return;
	
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (ACharacterBase* Character = Cast<ACharacterBase>(Pawn))
	{
		ActiveBattleSession.Reset();
		ActiveBattleSession.bSessionActive = true;
		ActiveBattleSession.ExplorationCharacterClass = Character->GetClass();
		ActiveBattleSession.ExplorationCharacterTransform = Character->GetActorTransform();

		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			ActiveBattleSession.ExplorationControlRotation = PC->GetControlRotation();
		}

		if (USpringArmComponent* Boom = Character->GetCameraBoom())
		{
			ActiveBattleSession.ExplorationArmLength = Boom->TargetArmLength;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[GameContextManager]: Saved exploration character: %s"),
			*GetNameSafe(Character));
		
		DestroyExplorationCharacter(Character);
	}
	else
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GameContextManager]: Failed to save exploration character. Pawn=%s"),
			*GetNameSafe(Pawn));
	}

	ActiveBattleSession.EncounterSource = EncounterSource;

	EnterState(EGameContextState::BattlePreparation);

	// Create grid manager (empty, not initialized yet)
	UGridManager* Grid = GetWorld()->GetSubsystem<UGridManager>();

	// Create battle preparation context
	ActiveBattlePrepContext = NewObject<UBattlePreparationContext>(this);
	ActiveBattlePrepContext->Initialize();

	// Create combat manager
	CreateCombatManager();

	// 1. Generate encounter data
	auto* EncounterGen = GetWorld()->GetSubsystem<UEncounterGeneratorSubsystem>();
	EncounterGen->GenerateEncounter(
		EncounterSource->EncounterData,
		*ActiveBattlePrepContext
	);

	// 2. Generate player party
	auto* PartyManager =
		GetGameInstance()->GetSubsystem<UPartyManagerSubsystem>();

	PartyManager->BuildBattleParty(
		EncounterSource->EncounterData,
		ActiveBattlePrepContext->PlayerParty
	);
	
	ActiveBattleSession.GeneratedParty = ActiveBattlePrepContext->PlayerParty;
	ActiveBattleSession.TotalEnemies = ActiveBattlePrepContext->EnemyParty.Num();

	// 3. Generate tactical grid directly from the encounter volume's BattleBox
	GetWorld()->GetSubsystem<UGridManager>()->GenerateGrid(EncounterSource, GetWorld());

	// 4. Plan battlefield layout
	auto* BattlefieldQuery =
		GetWorld()->GetSubsystem<UBattlefieldQuerySubsystem>();

	BattlefieldQuery->BuildCache();

	auto* Layout =
		GetWorld()->GetSubsystem<UBattlefieldLayoutSubsystem>();

	Layout->PlanSpawns(
		*BattlefieldQuery,
		*ActiveBattlePrepContext
	);

	// 5. Enter preparation phase
	ActiveCombatManager->EnterPreparation(*ActiveBattlePrepContext);

	UE_LOG(LogTemp, Warning, TEXT("[GameContextManager]: Battle preparation ready!"));

	// Debug auto-start
	RequestBattleStart();
}

void UGameContextManager::AbortBattle()
{
	ActiveCombatManager->CancelPreparation();
	ActiveBattlePrepContext = nullptr;
}

void UGameContextManager::RequestBattleStart()
{
	UE_LOG(LogTemp, Warning, TEXT("[GameContextManager]: Requesting battle start..."));
	if (!CanEnterState(EGameContextState::Battle))
		return;
	UE_LOG(LogTemp, Warning, TEXT("[GameContextManager]: Can enter battle state!"));
	if (!ActiveBattlePrepContext ||
		!ActiveBattlePrepContext->IsReadyToStartCombat())
	{
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[GameContextManager]: Battle preparation context is ready to start combat!"));

	EnterState(EGameContextState::Battle);

	check(ActiveCombatManager);

	ActiveCombatManager->StartCombat(*ActiveBattlePrepContext);

	// Context is now immutable and discarded
	ActiveBattlePrepContext = nullptr;
}

void UGameContextManager::RequestBattleEnd()
{
	if (ActiveCombatManager)
	{
		ActiveCombatManager->EndCombatWithWinner(EBattleUnitTeam::Enemy);
	}
}

void UGameContextManager::RequestTransition()
{
	EnterState(EGameContextState::Transition);
}

void UGameContextManager::EnterBattleResults()
{
	if (!CanEnterState(EGameContextState::BattleResults))
	{
		return;
	}

	// Build the context snapshot before entering the state so the
	// TowerFloorGameMode handler can pass it to the controller immediately.
	ActiveResultsContext = NewObject<UBattleResultsContext>(this);
	ActiveResultsContext->Initialize(ActiveBattleSession);
	ActiveResultsContext->DumpToLog();

	// Enter state — TowerFloorGameMode::HandleGameContextChanged spawns
	// BattleResultsController and calls SetResultsContext on it.
	EnterState(EGameContextState::BattleResults);
}

void UGameContextManager::ReturnToExploration()
{
	if (!CanEnterState(EGameContextState::Exploration))
	{
		UE_LOG(LogTemp, Error, TEXT("[GameContextManager]: Cannot return to exploration."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) { return; }

	ATowerFloorGameMode* GM = GetActiveFloorGameMode();
	if (!GM) { return; }

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC) { return; }

	// Spawn the exploration character before entering Exploration state so
	// TowerFloorGameMode::HandleGameContextChanged can possess it immediately.
	ACharacterBase* NewCharacter = World->SpawnActor<ACharacterBase>(
		ActiveBattleSession.ExplorationCharacterClass,
		ActiveBattleSession.ExplorationCharacterTransform);

	if (!IsValid(NewCharacter))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GameContextManager]: Failed to respawn exploration character."));
		return;
	}

	ActiveBattleSession.ExplorationCharacter = NewCharacter;
	ActiveResultsContext = nullptr;

	EnterState(EGameContextState::Exploration);
}

bool UGameContextManager::CanEnterState(EGameContextState TargetState) const
{
	const ATowerFloorGameMode* FloorGM = GetActiveFloorGameMode();
	if (!FloorGM)
		return false;
	
	switch (TargetState)
	{
	case EGameContextState::Exploration:
		return FloorGM->AllowsExploration();

	case EGameContextState::BattlePreparation:
	case EGameContextState::Battle:
		return FloorGM->AllowsCombat();
	case EGameContextState::BattleResults:

	default:
		return true;
	}
}

void UGameContextManager::EnterState(EGameContextState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	const EGameContextState OldState = CurrentState;

	ExitState(OldState);
	CurrentState = NewState;

	UE_LOG(LogTemp, Warning, TEXT("[GameContextManager]: Broadcasting GameContextChanged!"));
	OnGameContextChanged.Broadcast(NewState);
}

void UGameContextManager::ExitState(EGameContextState OldState)
{
	// Destroy / disable systems related to OldState
}

void UGameContextManager::CreateCombatManager()
{
	if (ActiveCombatManager)
	{
		ActiveCombatManager->Cleanup();
		ActiveCombatManager = nullptr;
	}

	ActiveCombatManager = NewObject<UCombatManager>(this);
	ActiveCombatManager->Initialize();
	ActiveCombatManager->OnCombatFinished.AddDynamic(this, &UGameContextManager::HandleCombatFinished);
}

void UGameContextManager::HandleCombatFinished(EBattleUnitTeam WinningTeam)
{
	if (!ActiveCombatManager)
	{
		return;
	}

	ActiveBattleSession.WinningTeam    = WinningTeam;
	ActiveBattleSession.bCombatFinished = true;
	ActiveBattleSession.SurvivingAllies  = ActiveCombatManager->GetAliveAllies();
	ActiveBattleSession.SurvivingEnemies = ActiveCombatManager->GetAliveEnemies();

	if (ActiveBattleSession.EncounterSource.IsValid())
	{
		ActiveBattleSession.EncounterSource->DeactivateEncounter();
	}
	
	if (UPartyManagerSubsystem* PartyManager =
		GetGameInstance()->GetSubsystem<UPartyManagerSubsystem>())
	{
		PartyManager->WriteBackBattleResults(
			ActiveCombatManager->GetSpawnedAllies(),
			ActiveBattleSession.GeneratedParty
		);
	}

	ActiveCombatManager->Cleanup();
	ActiveCombatManager = nullptr;

	EnterBattleResults();
}

UBattlePreparationContext* UGameContextManager::GetActivePrepContext() const
{
	return ActiveBattlePrepContext;
}

UBattleResultsContext* UGameContextManager::GetActiveResultsContext() const
{
	return ActiveResultsContext;
}

ATowerFloorGameMode* UGameContextManager::GetActiveFloorGameMode() const
{
	if (!GetWorld())
		return nullptr;

	return GetWorld()->GetAuthGameMode<ATowerFloorGameMode>();
}

ACharacterBase* UGameContextManager::GetSavedExplorationCharacter() const
{
	return ActiveBattleSession.ExplorationCharacter;
}

FTransform UGameContextManager::GetSavedExplorationTransform() const
{
	return ActiveBattleSession.ExplorationCharacterTransform;
}

void UGameContextManager::ClearBattleSession()
{
	ActiveBattleSession.Reset();
}

FRotator UGameContextManager::GetSavedExplorationControlRotation() const
{
	return ActiveBattleSession.ExplorationControlRotation;
}

void UGameContextManager::DestroyExplorationCharacter(ACharacterBase* Character)
{
	if (!Character)
	{
		return;
	}

	if (AController* Controller = Character->GetController())
	{
		Controller->UnPossess();
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[GameContextManager]: Destroying exploration character: %s"),
		*GetNameSafe(Character));

	Character->Destroy();
}