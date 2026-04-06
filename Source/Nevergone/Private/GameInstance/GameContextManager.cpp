// Copyright Xyzto Works


#include "GameInstance/GameContextManager.h"

#include "EngineUtils.h"
#include "Characters/CharacterBase.h"
#include "GameMode/TowerFloorGameMode.h"
#include "GameMode/CombatManager.h"
#include "GameMode/BattlePreparationContext.h"
#include "Actors/BattlefieldSpawnZone.h"
#include "GameMode/BattlefieldLayoutSubsystem.h"
#include "GameMode/BattlefieldQuerySubsystem.h"
#include "GameMode/BattleResultsContext.h"
#include "GameInstance/MyGameInstance.h"
#include "Level/EncounterGeneratorSubsystem.h"
#include "Level/FloorEncounterVolume.h"
#include "Level/GridManager.h"
#include "Party/PartyManagerSubsystem.h"
#include "ActorComponents/SaveableComponent.h"
#include "Types/LevelTypes.h"

#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameMode/BattleResultsContext.h"
#include "Kismet/GameplayStatics.h"


void UGameContextManager::RequestInitialState(EGameContextState InitialState)
{
    // If a mid-combat restore was completed by HandleSaveLoaded, ignore the
    // GameMode's requested initial state and enter Battle instead.
    // This handles the timing gap where TowerFloorGameMode::BeginPlay fires
    // AFTER HandleWorldInitializedActors has already wired the combat session.
    if (bPendingCombatRestore)
    {
        bPendingCombatRestore = false;

        UE_LOG(LogTemp, Warning,
            TEXT("[GameContextManager] RequestInitialState: mid-combat restore pending — entering Battle instead of state %d"),
            (int32)InitialState);

        EnterState(EGameContextState::Battle);
        return;
    }

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

	// NOTE: EnterState is called LAST in this function, after the context is fully
	// populated. The broadcast fires HandleGameContextChanged on TowerFloorGameMode,
	// which immediately calls SetPreparationContext() on BattlePreparationController —
	// so ActiveBattlePrepContext must be ready before that happens.

	// Create battle preparation context
	ActiveBattlePrepContext = NewObject<UBattlePreparationContext>(this);
	ActiveBattlePrepContext->Initialize();
	ActiveBattlePrepContext->EncounterSource = EncounterSource;

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
	UGridManager* Grid = GetWorld()->GetSubsystem<UGridManager>();
	Grid->GenerateGrid(EncounterSource, GetWorld());

	// 4. Plan battlefield layout
	auto* BattlefieldQuery =
		GetWorld()->GetSubsystem<UBattlefieldQuerySubsystem>();

	BattlefieldQuery->BuildCache();

	// Guard: if either team has no spawn zones in this level, auto-abort and
	// mark the encounter as resolved so the player is not softlocked.
	{
		TArray<ABattlefieldSpawnZone*> EnemyZones;
		TArray<ABattlefieldSpawnZone*> AllyZones;
		BattlefieldQuery->GetSpawnZones(ESpawnZoneTeam::Enemy, EnemyZones);
		BattlefieldQuery->GetSpawnZones(ESpawnZoneTeam::Ally, AllyZones);

		const bool bMissingEnemy = EnemyZones.Num() == 0;
		const bool bMissingAlly  = AllyZones.Num()  == 0;

		if (bMissingEnemy || bMissingAlly)
		{
			UE_LOG(LogTemp, Error,
				TEXT("[GameContextManager] RequestBattlePreparation: missing spawn zones — "
				     "Enemy zones: %d, Ally zones: %d. "
				     "Auto-aborting encounter and marking it as resolved. "
				     "Place at least one BP_EnemySpawnZone and one BP_AllySpawnZone inside the BattleBox."),
				EnemyZones.Num(), AllyZones.Num());

			EncounterSource->DeactivateEncounter();

			ActiveCombatManager->Cleanup();
			ActiveCombatManager = nullptr;
			ActiveBattlePrepContext = nullptr;
			ActiveBattleSession.ResetCombatResults();

			// CurrentState is still Exploration here (EnterState(BattlePreparation)
			// has not been called yet). ReturnToExploration() would silently no-op
			// because EnterState guards against re-entering the current state.
			// ForceReenterExploration() bypasses that guard and broadcasts directly.
			ForceReenterExploration();
			return;
		}
	}

	auto* Layout =
		GetWorld()->GetSubsystem<UBattlefieldLayoutSubsystem>();

	Layout->PlanSpawns(
		*BattlefieldQuery,
		*Grid,
		*ActiveBattlePrepContext
	);

	// 5. Enter preparation phase on the combat manager
	ActiveCombatManager->EnterPreparation(*ActiveBattlePrepContext);


	UE_LOG(LogTemp, Log, TEXT("[GameContextManager] Battle preparation ready — handing control to BattlePreparationController"));
	EnterState(EGameContextState::BattlePreparation);
}

void UGameContextManager::RequestAbortBattle()
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

void UGameContextManager::ForceReenterExploration()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	// Spawn the saved exploration character.
	ACharacterBase* NewCharacter = World->SpawnActor<ACharacterBase>(
		ActiveBattleSession.ExplorationCharacterClass,
		ActiveBattleSession.ExplorationCharacterTransform);

	if (!IsValid(NewCharacter))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GameContextManager] ForceReenterExploration: failed to spawn exploration character."));
		return;
	}

	ActiveBattleSession.ExplorationCharacter = NewCharacter;
	ActiveResultsContext = nullptr;

	// Update CurrentState unconditionally so listeners see the correct value.
	CurrentState = EGameContextState::Exploration;

	// Broadcast directly — bypasses EnterState's same-state early-return,
	// which would silently drop the event when CurrentState is already Exploration.
	UE_LOG(LogTemp, Warning, TEXT("[GameContextManager] ForceReenterExploration: broadcasting Exploration state."));
	OnGameContextChanged.Broadcast(EGameContextState::Exploration);
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

	// -- Abort: discard everything and return to exploration with no side effects
	if (WinningTeam == EBattleUnitTeam::None)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameContextManager] Combat aborted — discarding combat results and returning to exploration."));
		ActiveCombatManager->Cleanup();
		ActiveCombatManager = nullptr;
		ActiveBattleSession.ResetCombatResults(); // Preserves exploration restore data
		ReturnToExploration();
		return;
	}

	ActiveBattleSession.WinningTeam     = WinningTeam;
	ActiveBattleSession.bCombatFinished = true;
	ActiveBattleSession.SurvivingAllies  = ActiveCombatManager->GetAliveAllies();
	ActiveBattleSession.SurvivingEnemies = ActiveCombatManager->GetAliveEnemies();

	// -- Player won: deactivate the encounter so it won't trigger again
	if (WinningTeam == EBattleUnitTeam::Ally)
	{
		if (ActiveBattleSession.EncounterSource.IsValid())
		{
			ActiveBattleSession.EncounterSource->DeactivateEncounter();
		}
	}
	// -- Enemy won: keep the encounter active so the player can retry it

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
// ---------------------------------------------------------------------------
// Mid-combat save restore
// ---------------------------------------------------------------------------

void UGameContextManager::HandleSaveLoaded()
{
    UMyGameInstance* MyGI = Cast<UMyGameInstance>(GetGameInstance());
    if (!MyGI) { return; }

    UMySaveGame* Save = MyGI->GetActiveSave();
    if (!Save || !Save->bSavedMidCombat) { return; }

    // This is called by WorldManagerSubsystem::HandleWorldInitializedActors
    // after RestoreWorldState() completes, so the world and all actors
    // (including EncounterVolume) are guaranteed to be ready.
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[GameContextManager] HandleSaveLoaded: World is null — cannot restore combat"));
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[GameContextManager] HandleSaveLoaded: mid-combat save detected — restoring battle"));

    RequestBattleCombatRestore(Save->SavedCombatSession);
}

void UGameContextManager::RequestBattleCombatRestore(const FSavedCombatSession& SavedSession)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[GameContextManager] RequestBattleCombatRestore: reconstructing combat from save"));

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[GameContextManager] RequestBattleCombatRestore: World is null — aborting"));
        return;
    }

    // --- 1. Find the EncounterVolume by its SaveableComponent guid ---
    AFloorEncounterVolume* EncounterVolume = nullptr;
    for (TActorIterator<AFloorEncounterVolume> It(World); It; ++It)
    {
        USaveableComponent* SaveComp = It->FindComponentByClass<USaveableComponent>();
        if (SaveComp && SaveComp->GetOrCreateGuid() == SavedSession.EncounterVolumeGuid)
        {
            EncounterVolume = *It;
            break;
        }
    }

    if (!EncounterVolume)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[GameContextManager] RequestBattleCombatRestore: EncounterVolume with guid %s not found — aborting"),
            *SavedSession.EncounterVolumeGuid.ToString());
        return;
    }

    UE_LOG(LogTemp, Log,
        TEXT("[GameContextManager] RequestBattleCombatRestore: found EncounterVolume '%s'"),
        *GetNameSafe(EncounterVolume));

    // --- 2. Destroy the default pawn spawned by AGameModeBase on level start ---
    // In a combat restore there is no exploration session to save — the player
    // was mid-combat when they saved. We simply destroy whatever default pawn
    // the GameMode spawned so the BattlePlayerController can possess
    // BattleCameraPawn cleanly without a dangling exploration character.
    ActiveBattleSession.Reset();
    ActiveBattleSession.bSessionActive    = true;
    ActiveBattleSession.EncounterSource   = EncounterVolume;

    APawn* DefaultPawn = UGameplayStatics::GetPlayerPawn(World, 0);
    if (DefaultPawn)
    {
        if (APlayerController* PC = DefaultPawn->GetController<APlayerController>())
        {
            PC->UnPossess();
        }
        DefaultPawn->Destroy();
        UE_LOG(LogTemp, Log,
            TEXT("[GameContextManager] RequestBattleCombatRestore: destroyed default pawn '%s'"),
            *GetNameSafe(DefaultPawn));
    }

    // --- 3. Destroy any ACharacterBase actors left in the world ---
    // When a mid-combat save is loaded, the level is reloaded from disk.
    // Any ACharacterBase actors that existed at save time (bPersistAcrossLoads=false,
    // so not collected by WorldSaveData) still exist as physical geometry until
    // explicitly destroyed. StartCombatFromSave will spawn fresh units, so we must
    // clear all existing characters first to avoid collision failures and ghost actors.
    {
        TArray<ACharacterBase*> ToDestroy;
        for (TActorIterator<ACharacterBase> It(World); It; ++It)
        {
            ToDestroy.Add(*It);
        }
        for (ACharacterBase* Char : ToDestroy)
        {
            if (IsValid(Char))
            {
                UE_LOG(LogTemp, Log,
                    TEXT("[GameContextManager] RequestBattleCombatRestore: destroying leftover character '%s'"),
                    *GetNameSafe(Char));
                Char->Destroy();
            }
        }
    }

    // --- 4. Build BattlePreparationContext from saved encounter data ---
    ActiveBattlePrepContext = NewObject<UBattlePreparationContext>(this);
    ActiveBattlePrepContext->Initialize();
    ActiveBattlePrepContext->EncounterSource = EncounterVolume;

    // Generate encounter and party exactly as RequestBattlePreparation does —
    // unit data (class, level, HP) is read from UnitStatsComponent which was
    // already restored from save by WorldManagerSubsystem::RestoreWorldState.
    UEncounterGeneratorSubsystem* EncounterGen =
        World->GetSubsystem<UEncounterGeneratorSubsystem>();
    if (!EncounterGen)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[GameContextManager] RequestBattleCombatRestore: EncounterGeneratorSubsystem missing"));
        return;
    }
    EncounterGen->GenerateEncounter(
        EncounterVolume->EncounterData,
        *ActiveBattlePrepContext
    );

    UPartyManagerSubsystem* PartyManager =
        GetGameInstance()->GetSubsystem<UPartyManagerSubsystem>();
    if (PartyManager)
    {
        PartyManager->BuildBattleParty(
            EncounterVolume->EncounterData,
            ActiveBattlePrepContext->PlayerParty
        );
    }

    ActiveBattleSession.GeneratedParty  = ActiveBattlePrepContext->PlayerParty;
    ActiveBattleSession.TotalEnemies    = ActiveBattlePrepContext->EnemyParty.Num();

    // Rebuild the grid from the encounter volume's BattleBox.
    UGridManager* Grid = World->GetSubsystem<UGridManager>();
    if (!Grid)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[GameContextManager] RequestBattleCombatRestore: GridManager missing"));
        return;
    }
    Grid->GenerateGrid(EncounterVolume, World);

    UBattlefieldQuerySubsystem* BattlefieldQuery =
        World->GetSubsystem<UBattlefieldQuerySubsystem>();
    if (!BattlefieldQuery)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[GameContextManager] RequestBattleCombatRestore: BattlefieldQuerySubsystem missing"));
        return;
    }
    BattlefieldQuery->BuildCache();

    UBattlefieldLayoutSubsystem* Layout =
        World->GetSubsystem<UBattlefieldLayoutSubsystem>();
    if (!Layout)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[GameContextManager] RequestBattleCombatRestore: BattlefieldLayoutSubsystem missing"));
        return;
    }
    Layout->PlanSpawns(*BattlefieldQuery, *Grid, *ActiveBattlePrepContext);

    // --- 5. Create CombatManager and spawn units ---
    CreateCombatManager();
    ActiveCombatManager->EnterPreparation(*ActiveBattlePrepContext);

    // --- 6. Apply TemporaryAttributeBonuses before BattleState initializes ---
    // RestoreCombatFromSave writes TemporaryAttributeBonuses onto UnitStatsComponent
    // so that RecalculateBattleStats (called inside StartCombatFromSave) produces
    // the correct derived stats.
    ActiveCombatManager->RestoreCombatFromSave(SavedSession);

    // --- 7. Start combat using the save-aware initialization path ---
    // This calls BattleState::InitializeFromSave instead of Initialize,
    // restoring CurrentHP, CurrentAP, and ActiveStatusEffects without
    // re-applying passive effects.
    ActiveCombatManager->StartCombatFromSave(*ActiveBattlePrepContext, SavedSession);

    ActiveBattlePrepContext = nullptr;

    // Signal that combat has been fully wired from a save.
    // RequestInitialState (called by TowerFloorGameMode::BeginPlay shortly after)
    // will detect this flag and enter Battle instead of the GameMode's default
    // initial state. We do NOT broadcast here because TowerFloorGameMode has
    // not yet subscribed to OnGameContextChanged at this point in the pipeline.
    bPendingCombatRestore = true;

    UE_LOG(LogTemp, Warning,
        TEXT("[GameContextManager] RequestBattleCombatRestore: combat wired — waiting for RequestInitialState to enter Battle"));
}