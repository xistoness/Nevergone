// Copyright Xyzto Works


#include "GameMode/CombatManager.h"

#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "GameMode/BattlePreparationContext.h"
#include "GameMode/Combat/BattleInputManager.h"
#include "GameMode/Combat/BattleCameraPawn.h"
#include "Characters/CharacterBase.h"
#include "Characters/PlayerControllers/BattlePlayerController.h"
#include "GameMode/TurnManager.h"
#include "GameMode/Combat/BattleInputContextBuilder.h"
#include "GameMode/Combat/BattleState.h"
#include "Level/GridManager.h"
#include "Types/BattleTypes.h"

void UCombatManager::Initialize()
{
	// Setup participants
}

void UCombatManager::EnterPreparation(UBattlePreparationContext& BattlePrepContext)
{
	// Lock exploration input
	// Switch camera mode
	// Initialize preview systems
}

void UCombatManager::StartCombat(UBattlePreparationContext& BattlePrepContext)
{	
	UE_LOG(LogTemp, Warning, TEXT("=== StartCombat ==="));
	LogActivePlayerController(GetWorld(), TEXT("StartCombat BEGIN"));
	
	UGridManager* Grid = GetWorld()->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatManager]: Grid is invalid..."));
		return;
	}
	
	// Spawn allies
	for (int32 i = 0; i < BattlePrepContext.PlayerPlannedSpawns.Num(); ++i)
	{
		const FPlannedSpawn& Spawn = BattlePrepContext.PlayerPlannedSpawns[i];

		AActor* SpawnedAlly = GetWorld()->SpawnActor<AActor>(
			Spawn.ActorClass,
			Spawn.PlannedTransform
		);

		ACharacterBase* Character = Cast<ACharacterBase>(SpawnedAlly);
		if (!Character)
		{
			continue;
		}

		SpawnedAllies.Add(Character);
		Grid->RegisterActorToGrid(Character);
		UUnitStatsComponent* CharacterUnitStats = Character->GetUnitStats();
		CharacterUnitStats->SetCurrentActionPoints(CharacterUnitStats->GetActionPoints());

		FSpawnedBattleUnit Entry;
		Entry.UnitActor = Character;
		Entry.Team = EBattleUnitTeam::Player;
		Entry.SourceIndex = i;
		
		BattlePrepContext.SpawnedUnits.Add(Entry);
	}

	// Spawn enemies
	for (int32 i = 0; i < BattlePrepContext.EnemyPlannedSpawns.Num(); ++i)
	{
		const FPlannedSpawn& Spawn = BattlePrepContext.EnemyPlannedSpawns[i];

		AActor* SpawnedEnemy = GetWorld()->SpawnActor<AActor>(
			Spawn.ActorClass,
			Spawn.PlannedTransform
		);

		ACharacterBase* Character = Cast<ACharacterBase>(SpawnedEnemy);
		if (!Character)
		{
			continue;
		}
		
		SpawnedEnemies.Add(Character);
		Grid->RegisterActorToGrid(Character);
		UUnitStatsComponent* CharacterUnitStats = Character->GetUnitStats();
		CharacterUnitStats->SetCurrentActionPoints(CharacterUnitStats->GetActionPoints());

		FSpawnedBattleUnit Entry;
		Entry.UnitActor = Character;
		Entry.Team = EBattleUnitTeam::Enemy;
		Entry.SourceIndex = i;
		
		BattlePrepContext.SpawnedUnits.Add(Entry);
	}

	TArray<AActor*> AllCombatants;
	AllCombatants.Append(SpawnedAllies);
	AllCombatants.Append(SpawnedEnemies);

	// Create turn manager
	TurnManager = NewObject<UTurnManager>(this);
	TurnManager->Initialize(AllCombatants);

	// Create input manager
	BattleInputManager = NewObject<UBattleInputManager>(this);
	BattleInputManager->Initialize(TurnManager, BattleCameraPawn, this);
	
	// Create input context builder
	InputContextBuilder = NewObject<UBattleInputContextBuilder>(this);
	InputContextBuilder->SetTurnManager(TurnManager);
	UpdateInputContext();
	
	// Create battle state data
	BattleState = NewObject<UBattleState>();
	BattleState->Initialize(BattlePrepContext);

	// Bind turn events
	TurnManager->OnTurnStateChanged.AddUObject(
	this,
	&UCombatManager::HandleTurnStateChanged
	);
	
	// Tells the Battle Player Controller to start battle
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (ABattlePlayerController* BattlePC =
			Cast<ABattlePlayerController>(PC))
		{
			BattlePC->EnterBattleMode(this);
		}
	}
	
	//  Tells the Turn Manager to start combat and trigger first turn
	TurnManager->StartCombat();
}

void UCombatManager::HandleTurnStateChanged(
	EBattleTurnOwner NewOwner,
	EBattleTurnPhase NewPhase)
{
	if (NewOwner == EBattleTurnOwner::Player &&
		NewPhase == EBattleTurnPhase::AwaitingOrders)
	{
		OnPlayerTurnStarted();
	}
	else if (NewOwner == EBattleTurnOwner::Enemy &&
		NewPhase == EBattleTurnPhase::ExecutingActions)
	{
		OnEnemyTurnStarted();
	}
}

void UCombatManager::UpdateInputContext()
{
	if (!InputContextBuilder || !BattleInputManager)
	{
		return;
	}

	const FBattleInputContext Context =
		InputContextBuilder->BuildContext();

	BattleInputManager->SetInputContext(Context);
}

void UCombatManager::RegisterBattleCamera(ABattleCameraPawn* InCameraPawn)
{
	BattleCameraPawn = InCameraPawn;

	if (BattleInputManager)
	{
		// Late binding safety
		BattleInputManager->Initialize(TurnManager, BattleCameraPawn, this);
	}
}

void UCombatManager::OnPlayerTurnStarted()
{
	InputContextBuilder->SetCameraInputEnabled(true);
	InputContextBuilder->SetHardLock(false);
	UpdateInputContext();
	SelectedUnit = INDEX_NONE;
	SelectFirstAvailableUnit();
}

void UCombatManager::OnEnemyTurnStarted()
{
	InputContextBuilder->SetCameraInputEnabled(false);
	InputContextBuilder->SetHardLock(true);
	UpdateInputContext();
}

void UCombatManager::SelectFirstAvailableUnit()
{
	for (int32 i = 0; i < SpawnedAllies.Num(); ++i)
	{
		ACharacterBase* Unit = Cast<ACharacterBase>(SpawnedAllies[i]);
		if (!Unit)
		{
			continue;
		}

		if (BattleState && BattleState->CanUnitAct(Unit))
		{
			SelectedUnit = i;
			HandleActiveUnitChanged(Unit);
			return;
		}
	}
	
	// No valid unit found
	EndPlayerTurn();	
}

void UCombatManager::SelectNextControllableUnit()
{
	if (SpawnedAllies.Num() == 0 || !BattleState)
	{
		return;
	}
	
	if (SelectedUnit == INDEX_NONE)
	{
		SelectFirstAvailableUnit();
		return;
	}

	const int32 StartIndex = SelectedUnit;
	int32 Index = SelectedUnit;

	do
	{
		Index = (Index + 1) % SpawnedAllies.Num();

		ACharacterBase* Unit = Cast<ACharacterBase>(SpawnedAllies[Index]);
		if (Unit && BattleState->CanUnitAct(Unit))
		{
			SelectedUnit = Index;
			HandleActiveUnitChanged(Unit);
			return;
		}

	} while (Index != StartIndex);
	
	EndPlayerTurn();
}

void UCombatManager::SelectPreviousControllableUnit()
{
	if (SpawnedAllies.Num() == 0 || !BattleState)
	{
		return;
	}
	
	if (SelectedUnit == INDEX_NONE)
	{
		SelectFirstAvailableUnit();
		return;
	}

	const int32 Count = SpawnedAllies.Num();
	const int32 StartIndex = SelectedUnit;

	int32 Index = SelectedUnit;

	do
	{
		Index = (Index - 1 + Count) % Count;

		ACharacterBase* Unit = Cast<ACharacterBase>(SpawnedAllies[Index]);
		if (Unit && BattleState->CanUnitAct(Unit))
		{
			SelectedUnit = Index;
			HandleActiveUnitChanged(Unit);
			return;
		}

	} while (Index != StartIndex);

	EndPlayerTurn();
}


void UCombatManager::HandleActiveUnitChanged(ACharacterBase* NewActiveUnit)
{
	UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Tries to handle active unit changed..."));
	if (InputContextBuilder)
	{
		InputContextBuilder->SetInteractionMode(EBattleInteractionMode::Targeting);
		UpdateInputContext();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatManager] InputContextBuilder invalid"));
	}
	
	NewActiveUnit->GetBattleModeComponent()->OnActionPointsDepleted.AddUObject(this, &UCombatManager::HandleUnitOutOfAP);
	
	if (BattleInputManager)
	{
		BattleInputManager->OnActiveUnitChanged(NewActiveUnit);
	}
}

void UCombatManager::HandleUnitOutOfAP(ACharacterBase* Unit)
{
	SelectNextControllableUnit();
}

void UCombatManager::EndPlayerTurn()
{
	UE_LOG(LogTemp, Log, TEXT("[CombatManager] No units left. Ending player turn."));
	
	SelectedUnit = INDEX_NONE;

	if (TurnManager)
	{
		TurnManager->EndCurrentTurn();
	}
}

void UCombatManager::CancelPreparation()
{
	Cleanup();
}

void UCombatManager::EndCombat()
{
	Cleanup();
	OnCombatFinished.Broadcast();
}

void UCombatManager::Cleanup()
{
	if (TurnManager)
	{
		TurnManager = nullptr;
	}

	BattleInputManager = nullptr;
	BattleCameraPawn = nullptr;
	BattleState = nullptr;

	SpawnedAllies.Empty();
	SpawnedEnemies.Empty();
	
}



void UCombatManager::LogActivePlayerController(UWorld* World, const FString& Context)
{
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] World is null"), *Context);
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();

	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] No PlayerController found"), *Context);
		return;
	}

	APawn* Pawn = PC->GetPawn();

	UE_LOG(LogTemp, Log,
	TEXT("[%s] PC=%s | Pawn=%s | Local=%s | Paused=%s"),
	*Context,
	*PC->GetName(),
	Pawn ? *Pawn->GetName() : TEXT("None"),
	PC->IsLocalController() ? TEXT("Yes") : TEXT("No"),
	PC->IsPaused() ? TEXT("Yes") : TEXT("No")
	);
}
