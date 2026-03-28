// Copyright Xyzto Works


#include "GameMode/CombatManager.h"

#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "GameInstance/MyGameInstance.h"
#include "GameMode/BattlePreparationContext.h"
#include "GameMode/Combat/BattleInputManager.h"
#include "GameMode/Combat/BattleCameraPawn.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "GameMode/Combat/AI/BattleTeamAIPlanner.h"
#include "Characters/CharacterBase.h"
#include "Characters/PlayerControllers/BattlePlayerController.h"
#include "GameMode/TurnManager.h"
#include "GameMode/Combat/BattleInputContextBuilder.h"
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

void UCombatManager::SpawnAllies(UBattlePreparationContext& BattlePrepContext, UGridManager* Grid)
{
	for (int32 i = 0; i < BattlePrepContext.PlayerPlannedSpawns.Num(); ++i)
	{
		const FPlannedSpawn& Spawn = BattlePrepContext.PlayerPlannedSpawns[i];

		FIntPoint SpawnCoord;
		if (!Grid->FindClosestValidTileToWorld(Spawn.PlannedTransform.GetLocation(), SpawnCoord, true))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[CombatManager]: Failed to find valid ally spawn tile for index %d"),
				i
			);
			continue;
		}

		FTransform FinalSpawnTransform = Spawn.PlannedTransform;
		FinalSpawnTransform.SetLocation(Grid->GetTileCenterWorld(SpawnCoord));

		AActor* SpawnedAlly = GetWorld()->SpawnActor<AActor>(
			Spawn.ActorClass,
			FinalSpawnTransform
		);

		ACharacterBase* Character = Cast<ACharacterBase>(SpawnedAlly);
		if (!Character)
		{
			continue;
		}

		BindToCombatUnitActionEvents(Character);
		SpawnedAllies.Add(Character);

		UUnitStatsComponent* CharacterUnitStats = Character->GetUnitStats();
		if (CharacterUnitStats)
		{
			CharacterUnitStats->SetCurrentActionPoints(CharacterUnitStats->GetActionPoints());
			// Death notification is now handled by EventBus::OnUnitDied —
			// no direct OnUnitDeath binding here
		}

		// Inject the event bus so this unit's abilities can route through it
		if (UBattleModeComponent* BattleComp = Character->GetBattleModeComponent())
		{
			BattleComp->SetCombatEventBus(EventBus);
		}

		FSpawnedBattleUnit Entry;
		Entry.UnitActor = Character;
		Entry.SourceIndex = i;
		Entry.Team = EBattleUnitTeam::Ally;

		InitializeSpawnedUnitForBattle(Character, EBattleUnitTeam::Ally, Grid);

		BattlePrepContext.SpawnedUnits.Add(Entry);
		
		const FVector TileCenter = Grid->GridToWorld(SpawnCoord);

		UE_LOG(LogTemp, Warning, TEXT("TileCenter: %s"), *TileCenter.ToString());
		UE_LOG(LogTemp, Warning, TEXT("ActorLocation: %s"), *Character->GetActorLocation().ToString());

		DrawDebugSphere(GetWorld(), TileCenter, 20.f, 12, FColor::Yellow, false, 10.f);
		DrawDebugSphere(GetWorld(), Character->GetActorLocation(), 20.f, 12, FColor::Blue, false, 10.f);
		
	}
}

void UCombatManager::SpawnEnemies(UBattlePreparationContext& BattlePrepContext, UGridManager* Grid)
{
	for (int32 i = 0; i < BattlePrepContext.EnemyPlannedSpawns.Num(); ++i)
	{
		const FPlannedSpawn& Spawn = BattlePrepContext.EnemyPlannedSpawns[i];

		FIntPoint SpawnCoord;
		if (!Grid->FindClosestValidTileToWorld(Spawn.PlannedTransform.GetLocation(), SpawnCoord, true))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[CombatManager]: Failed to find valid enemy spawn tile for index %d"),
				i
			);
			continue;
		}

		FTransform FinalSpawnTransform = Spawn.PlannedTransform;
		FinalSpawnTransform.SetLocation(Grid->GetTileCenterWorld(SpawnCoord));

		AActor* SpawnedEnemy = GetWorld()->SpawnActor<AActor>(
			Spawn.ActorClass,
			FinalSpawnTransform
		);

		ACharacterBase* Character = Cast<ACharacterBase>(SpawnedEnemy);
		if (!Character)
		{
			continue;
		}

		BindToCombatUnitActionEvents(Character);
		SpawnedEnemies.Add(Character);

		UUnitStatsComponent* CharacterUnitStats = Character->GetUnitStats();
		if (CharacterUnitStats)
		{
			CharacterUnitStats->SetCurrentActionPoints(CharacterUnitStats->GetActionPoints());
			// Death notification is now handled by EventBus::OnUnitDied —
			// no direct OnUnitDeath binding here
		}

		// Inject the event bus so this unit's abilities can route through it
		if (UBattleModeComponent* BattleComp = Character->GetBattleModeComponent())
		{
			BattleComp->SetCombatEventBus(EventBus);
		}

		FSpawnedBattleUnit Entry;
		Entry.UnitActor = Character;
		Entry.SourceIndex = i;
		Entry.Team = EBattleUnitTeam::Enemy;
		
		InitializeSpawnedUnitForBattle(Character, EBattleUnitTeam::Enemy, Grid);

		BattlePrepContext.SpawnedUnits.Add(Entry);
		
		const FVector TileCenter = Grid->GridToWorld(SpawnCoord);

		UE_LOG(LogTemp, Warning, TEXT("TileCenter: %s"), *TileCenter.ToString());
		UE_LOG(LogTemp, Warning, TEXT("ActorLocation: %s"), *Character->GetActorLocation().ToString());

		DrawDebugSphere(GetWorld(), TileCenter, 20.f, 12, FColor::Yellow, false, 10.f);
		DrawDebugSphere(GetWorld(), Character->GetActorLocation(), 20.f, 12, FColor::Blue, false, 10.f);
	}
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
	
	// EventBus must exist before spawning so units receive the reference
	// during SetCombatEventBus() calls inside SpawnAllies/SpawnEnemies.
	// BattleState is initialized after spawning because Initialize() reads
	// BattlePrepContext.SpawnedUnits, which is populated during the spawn loop.
	EventBus = NewObject<UCombatEventBus>(this);
	EventBus->OnUnitDied.AddUObject(this, &UCombatManager::HandleUnitDeath);

	SpawnAllies(BattlePrepContext, Grid);
	SpawnEnemies(BattlePrepContext, Grid);

	TArray<AActor*> AllCombatants;
	AllCombatants.Append(SpawnedAllies);
	AllCombatants.Append(SpawnedEnemies);

	// Create turn manager
	TurnManager = NewObject<UTurnManager>(this);
	TurnManager->Initialize(AllCombatants);
	
	// Create battle team AI planner
	BattleTeamAIPlanner = NewObject<UBattleTeamAIPlanner>(this);
	BattleTeamAIPlanner->Initialize(AllCombatants);

	// Create input manager
	BattleInputManager = NewObject<UBattleInputManager>(this);
	BattleInputManager->Initialize(TurnManager, BattleCameraPawn, this);
	
	// Create input context builder
	InputContextBuilder = NewObject<UBattleInputContextBuilder>(this);
	InputContextBuilder->SetTurnManager(TurnManager);
	UpdateInputContext();

	// BattleState is initialized here — SpawnedUnits is now fully populated
	BattleState = NewObject<UBattleState>();
	BattleState->Initialize(BattlePrepContext);

	// Wire BattleState into the bus now that both exist
	EventBus->Initialize(BattleState);

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

void UCombatManager::InitializeSpawnedUnitForBattle(ACharacterBase* Character, EBattleUnitTeam Team, UGridManager* Grid)
{
	if (!Character || !Grid)
	{
		return;
	}

	Grid->RegisterActorToGrid(Character);

	if (UUnitStatsComponent* UnitStats = Character->GetUnitStats())
	{
		UnitStats->SetCurrentActionPoints(UnitStats->GetActionPoints());

		switch (Team)
		{
		case EBattleUnitTeam::Ally:
			UnitStats->SetAllyTeam(0);
			UnitStats->SetEnemyTeam(1);
			break;	

		case EBattleUnitTeam::Enemy:
			UnitStats->SetAllyTeam(1);
			UnitStats->SetEnemyTeam(0);
			break;

		default:
			break;
		}
	}
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

void UCombatManager::HandleUnitDeath(ACharacterBase* DeadUnit)
{
	if (!DeadUnit)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[CombatManager]: Unit died: %s"), *DeadUnit->GetName());

	// Optional: remove from grid occupancy immediately
	if (UGridManager* Grid = GetWorld()->GetSubsystem<UGridManager>())
	{
		Grid->RemoveActorFromGrid(DeadUnit);
	}

	if (IsTeamDefeated(EBattleUnitTeam::Enemy))
	{
		EndCombatWithWinner(EBattleUnitTeam::Ally);
		return;
	}

	if (IsTeamDefeated(EBattleUnitTeam::Ally))
	{
		EndCombatWithWinner(EBattleUnitTeam::Enemy);
		return;
	}
}

bool UCombatManager::IsTeamDefeated(EBattleUnitTeam Team) const
{
	const TArray<ACharacterBase*>* TeamUnits = nullptr;

	switch (Team)
	{
	case EBattleUnitTeam::Ally:
		TeamUnits = &SpawnedAllies;
		break;

	case EBattleUnitTeam::Enemy:
		TeamUnits = &SpawnedEnemies;
		break;

	default:
		return false;
	}

	for (ACharacterBase* Unit : *TeamUnits)
	{
		if (!Unit)
		{
			continue;
		}

		if (const UUnitStatsComponent* Stats = Unit->GetUnitStats())
		{
			if (Stats->IsAlive())
			{
				return false;
			}
		}
	}

	return true;
}

void UCombatManager::UpdateInputContext()
{
	if (!InputContextBuilder || !BattleInputManager || bCombatEnding)
	{
		return;
	}

	const FBattleInputContext Context = InputContextBuilder->BuildContext();

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
	UE_LOG(LogTemp, Warning, TEXT("[CombatManager]: Player turn started"));
	if (!InputContextBuilder || bCombatEnding) return;
	
	ClearCurrentSelectedUnit();
	
	RestoreTeamActionPoints(EBattleUnitTeam::Ally);
	
	InputContextBuilder->SetUnitInputEnabled(true);
	InputContextBuilder->SetCameraInputEnabled(true);
	InputContextBuilder->SetHardLock(false);
	UpdateInputContext();
	
	SelectedUnit = INDEX_NONE;
	SelectFirstAvailableUnit();
}

void UCombatManager::OnEnemyTurnStarted()
{
	UE_LOG(LogTemp, Warning, TEXT("[CombatManager]: Enemy turn started"));
	if (!InputContextBuilder || bCombatEnding) return;
	
	ClearCurrentSelectedUnit();
	
	RestoreTeamActionPoints(EBattleUnitTeam::Enemy);

	InputContextBuilder->SetCameraInputEnabled(false);
	InputContextBuilder->SetHardLock(true);
	UpdateInputContext();
	
	BattleTeamAIPlanner->StartTeamTurn();
}

void UCombatManager::SelectFirstAvailableUnit()
{
	if (bCombatEnding) return;
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
	if (SpawnedAllies.Num() == 0 || !BattleState || bCombatEnding)
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
	if (bCombatEnding) return;
	
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
	if (!NewActiveUnit || !BattleCameraPawn || !TurnManager || bCombatEnding)
	{
		return;
	}

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

	if (CurrentSelectedUnit)
	{
		if (UBattleModeComponent* OldBattleComp = CurrentSelectedUnit->FindComponentByClass<UBattleModeComponent>())
		{
			OldBattleComp->OnActionPointsDepleted.RemoveDynamic(
				this,
				&UCombatManager::HandleUnitOutOfAP
			);
		}
	}

	CurrentSelectedUnit = NewActiveUnit;

	if (UBattleModeComponent* NewBattleComp = CurrentSelectedUnit->FindComponentByClass<UBattleModeComponent>())
	{
		NewBattleComp->OnActionPointsDepleted.AddUniqueDynamic(
			this,
			&UCombatManager::HandleUnitOutOfAP
		);
	}

	if (BattleInputManager)
	{
		BattleInputManager->OnActiveUnitChanged(CurrentSelectedUnit);
	}
}

void UCombatManager::HandleUnitOutOfAP(ACharacterBase* Unit)
{
	if (bCombatEnding || !TurnManager || !BattleState)
	{
		return;
	}

	if (TurnManager->GetCurrentTurnOwner() != EBattleTurnOwner::Player)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Ignoring OutOfAP: not player's turn."));
		return;
	}

	if (Unit != CurrentSelectedUnit)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Ignoring OutOfAP: unit is not current selected unit."));
		return;
	}

	SelectNextControllableUnit();
}

void UCombatManager::EndPlayerTurn()
{
	if (bCombatEnding || !TurnManager)
	{
		return;
	}

	if (TurnManager->GetCurrentTurnOwner() != EBattleTurnOwner::Player)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Ignoring EndPlayerTurn: current turn is not Player."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatManager] No units left. Ending player turn."));

	ClearCurrentSelectedUnit();

	if (BattleInputManager)
	{
		BattleInputManager->OnActiveUnitChanged(nullptr);
	}

	TurnManager->EndCurrentTurn();
}

void UCombatManager::EndEnemyTurn()
{
	if (bCombatEnding || !TurnManager)
	{
		return;
	}

	if (TurnManager->GetCurrentTurnOwner() != EBattleTurnOwner::Enemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Ignoring EndEnemyTurn: current turn is not Enemy."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatManager] No AI actions left. Finishing enemy turn."));

	ClearCurrentSelectedUnit();
	
	if (BattleInputManager)
	{
		BattleInputManager->OnActiveUnitChanged(nullptr);
	}
	
	if (BattleCameraPawn)
	{
		BattleCameraPawn->ClearLockOnActor();
	}
	
	if (InputContextBuilder)
	{
		InputContextBuilder->SetUnitInputEnabled(false);
		InputContextBuilder->SetCameraInputEnabled(true);
		InputContextBuilder->SetHardLock(false);		
	}
	
	TurnManager->EndCurrentTurn();
}

void UCombatManager::RequestEndEnemyTurn()
{
	EndEnemyTurn();
}

void UCombatManager::CancelPreparation()
{
	Cleanup();
}

void UCombatManager::ClearCurrentSelectedUnit()
{
	if (!CurrentSelectedUnit)
	{
		SelectedUnit = INDEX_NONE;
		return;
	}

	if (UBattleModeComponent* BattleComp = CurrentSelectedUnit->FindComponentByClass<UBattleModeComponent>())
	{
		BattleComp->OnActionPointsDepleted.RemoveAll(this);
	}

	CurrentSelectedUnit = nullptr;
	SelectedUnit = INDEX_NONE;
}

void UCombatManager::EndCombatWithWinner(EBattleUnitTeam WinningTeam)
{
	UE_LOG(LogTemp, Warning, TEXT("[CombatManager]: Combat ended. Winner team = %d"), (int32)WinningTeam);

	// Guard to prevent re-entry
	if (bCombatEnding)
	{
		return;
	}

	bCombatEnding = true;

	// Disable battle input immediately
	if (InputContextBuilder)
	{
		InputContextBuilder->SetCameraInputEnabled(false);
		InputContextBuilder->SetUnitInputEnabled(false);
	}

	// Trigger win/lose flow here

	OnCombatFinished.Broadcast(WinningTeam);
}

int32 UCombatManager::GetAliveAllies() const
{
	int32 Count = 0;

	for (ACharacterBase* Ally : SpawnedAllies)
	{
		if (!IsValid(Ally))
		{
			continue;
		}

		const UUnitStatsComponent* Stats = Ally->GetUnitStats();
		if (!Stats)
		{
			continue;
		}

		if (Stats->IsAlive())
		{
			++Count;
		}
	}

	return Count;
}

int32 UCombatManager::GetAliveEnemies() const
{
	int32 Count = 0;

	for (ACharacterBase* Enemy : SpawnedEnemies)
	{
		if (!IsValid(Enemy))
		{
			continue;
		}

		const UUnitStatsComponent* Stats = Enemy->GetUnitStats();
		if (!Stats)
		{
			continue;
		}

		if (Stats->IsAlive())
		{
			++Count;
		}
	}

	return Count;
}

void UCombatManager::RestoreTeamActionPoints(EBattleUnitTeam Team)
{
	if (bCombatEnding) return;
	
	const TArray<ACharacterBase*>* TeamUnits = nullptr;

	switch (Team)
	{
	case EBattleUnitTeam::Ally:
		TeamUnits = &SpawnedAllies;
		break;

	case EBattleUnitTeam::Enemy:
		TeamUnits = &SpawnedEnemies;
		break;

	default:
		return;
	}

	for (ACharacterBase* Unit : *TeamUnits)
	{
		if (!Unit)
		{
			continue;
		}

		UUnitStatsComponent* UnitStats = Unit->GetUnitStats();
		if (!UnitStats)
		{
			continue;
		}

		if (!UnitStats->IsAlive())
		{
			continue;
		}

		UnitStats->SetCurrentActionPoints(UnitStats->GetActionPoints());

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[CombatManager]: Restored AP for %s to %f"),
			*Unit->GetName(),
			UnitStats->GetActionPoints()
		);
	}
}

void UCombatManager::Cleanup()
{
	bCombatEnding = true;

	if (CurrentSelectedUnit)
	{
		if (UBattleModeComponent* BattleComp = CurrentSelectedUnit->FindComponentByClass<UBattleModeComponent>())
		{
			BattleComp->OnActionPointsDepleted.RemoveAll(this);
		}

		CurrentSelectedUnit = nullptr;
	}

	if (TurnManager)
	{
		TurnManager->OnTurnStateChanged.RemoveAll(this);
	}

	if (UGridManager* Grid = GetWorld()->GetSubsystem<UGridManager>())
	{
		for (ACharacterBase* Ally : SpawnedAllies)
		{
			if (IsValid(Ally))
			{
				UnbindFromCombatUnitActionEvents(Ally);
				Grid->RemoveActorFromGrid(Ally);
				UE_LOG(LogTemp, Error, TEXT("[CombatManager]: Destroying ally -> %s"), *GetNameSafe(Ally));
				Ally->Destroy();
			}
		}

		for (ACharacterBase* Enemy : SpawnedEnemies)
		{
			if (IsValid(Enemy))
			{
				UnbindFromCombatUnitActionEvents(Enemy);
				Grid->RemoveActorFromGrid(Enemy);
				UE_LOG(LogTemp, Error, TEXT("[CombatManager]: Destroying enemy -> %s"), *GetNameSafe(Enemy));
				Enemy->Destroy();
			}
		}
	}
	else
	{
		for (ACharacterBase* Ally : SpawnedAllies)
		{
			if (IsValid(Ally))
			{
				Ally->Destroy();
			}
		}

		for (ACharacterBase* Enemy : SpawnedEnemies)
		{
			if (IsValid(Enemy))
			{
				Enemy->Destroy();
			}
		}
	}

	SpawnedAllies.Empty();
	SpawnedEnemies.Empty();

	if (IsValid(BattleCameraPawn))
	{
		BattleCameraPawn->Destroy();
		BattleCameraPawn = nullptr;
	}

	SelectedUnit = INDEX_NONE;
	InputContextBuilder = nullptr;
	BattleInputManager = nullptr;
	TurnManager = nullptr;
	BattleState = nullptr;
	EventBus = nullptr;
}

void UCombatManager::BindToCombatUnitActionEvents(ACharacterBase* Unit)
{
	if (!Unit)
	{
		return;
	}

	if (UBattleModeComponent* BattleComp = Unit->FindComponentByClass<UBattleModeComponent>())
	{
		BattleComp->OnActionUseStarted.RemoveDynamic(this, &UCombatManager::HandleUnitActionStarted);
		BattleComp->OnActionUseFinished.RemoveDynamic(this, &UCombatManager::HandleUnitActionFinished);

		BattleComp->OnActionUseStarted.AddDynamic(this, &UCombatManager::HandleUnitActionStarted);
		BattleComp->OnActionUseFinished.AddDynamic(this, &UCombatManager::HandleUnitActionFinished);
	}
}

void UCombatManager::UnbindFromCombatUnitActionEvents(ACharacterBase* Unit)
{
	if (!Unit)
	{
		return;
	}

	if (UBattleModeComponent* BattleComp = Unit->FindComponentByClass<UBattleModeComponent>())
	{
		BattleComp->OnActionUseStarted.RemoveDynamic(this, &UCombatManager::HandleUnitActionStarted);
		BattleComp->OnActionUseFinished.RemoveDynamic(this, &UCombatManager::HandleUnitActionFinished);
	}
}

void UCombatManager::HandleUnitActionStarted(ACharacterBase* ActingUnit)
{
	if (bCombatEnding || !InputContextBuilder || !TurnManager)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Action started -> updating input state"));

	const EBattleTurnOwner TurnOwner = TurnManager->GetCurrentTurnOwner();

	if (TurnOwner == EBattleTurnOwner::Player)
	{
		InputContextBuilder->SetUnitInputEnabled(false);
		InputContextBuilder->SetCameraInputEnabled(true);
		InputContextBuilder->SetHardLock(false);
	}
	else
	{
		InputContextBuilder->SetUnitInputEnabled(false);
		InputContextBuilder->SetCameraInputEnabled(false);
		InputContextBuilder->SetHardLock(true);
		BattleCameraPawn->LockOnActor(ActingUnit);

		UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Action started -> focusing camera on acting enemy"));
		FocusCameraOnActingEnemy(ActingUnit);
	}

	UpdateInputContext();
}

void UCombatManager::HandleUnitActionFinished(ACharacterBase* ActingUnit)
{
	if (bCombatEnding || !InputContextBuilder || !TurnManager)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Action finished -> updating input state"));

	if (TurnManager->GetCurrentTurnOwner() == EBattleTurnOwner::Player)
	{
		InputContextBuilder->SetUnitInputEnabled(true);
		InputContextBuilder->SetCameraInputEnabled(true);
		InputContextBuilder->SetHardLock(false);

		// Restore targeting mode so the player can act again with the same unit
		// If the unit is out of AP, HandleUnitOutOfAP will have already
		// (or will shortly) call SelectNextControllableUnit — this is safe to call regardless
		if (ActingUnit && BattleState && BattleState->CanUnitAct(ActingUnit))
		{
			InputContextBuilder->SetInteractionMode(EBattleInteractionMode::Targeting);
		}
	}
	else
	{
		InputContextBuilder->SetUnitInputEnabled(false);
		InputContextBuilder->SetCameraInputEnabled(false);
		InputContextBuilder->SetHardLock(true);
		BattleCameraPawn->ClearLockOnActor();
	}

	UpdateInputContext();
}

void UCombatManager::FocusCameraOnActingEnemy(ACharacterBase* ActingUnit)
{
	if (!ActingUnit || !BattleInputManager)
	{
		return;
	}
	
	BattleCameraPawn = BattleInputManager->GetBattleCameraPawn();
	if (!IsValid(BattleCameraPawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Battle camera pawn is invalid!"));
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Locking camera to enemy unit"));

	const FVector FocusLocation = ActingUnit->GetActorLocation();
	BattleCameraPawn->FocusOnLocationSmooth(FocusLocation);
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