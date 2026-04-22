// Copyright Xyzto Works


#include "GameInstance/GameContextManager.h"

#include "ActorComponents/UnitStatsComponent.h"
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
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameMode/BattleResultsContext.h"
#include "Kismet/GameplayStatics.h"
#include "World/WorldManagerSubsystem.h"


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

			//Force return to exploration 
			//This needs to happen because state is already Exploration
			//Without forcing, CanEnterState blocks this return
			ReturnToExploration(true);
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
	ActiveCombatManager->CancelPreparation(ActiveBattleSession.EncounterSource);
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

void UGameContextManager::ReturnToExploration(bool bForce)
{
	// bForce is used when CurrentState is already Exploration but we still need
	// to re-broadcast — e.g. when battle preparation aborts before EnterState
	// was ever called, so the state never actually changed.
	if (!bForce && !CanEnterState(EGameContextState::Exploration))
	{
		UE_LOG(LogTemp, Error, TEXT("[GameContextManager] ReturnToExploration: cannot enter Exploration state."));
		return;
	}

	ACharacterBase* NewCharacter = SpawnExplorationActor();
	if (!IsValid(NewCharacter)) { return; }

	ActiveBattleSession.ExplorationCharacter = NewCharacter;
	ActiveResultsContext = nullptr;

	if (bForce)
	{
		// Bypass EnterState's same-state guard — update and broadcast directly.
		CurrentState = EGameContextState::Exploration;
		UE_LOG(LogTemp, Warning, TEXT("[GameContextManager] ReturnToExploration (forced): broadcasting Exploration state."));
		OnGameContextChanged.Broadcast(EGameContextState::Exploration);
	}
	else
	{
		EnterState(EGameContextState::Exploration);
	}
}

void UGameContextManager::SyncExplorationActorHP(ACharacterBase* ExplorationActor)
{
	if (!IsValid(ExplorationActor)) { return; }

	UUnitStatsComponent* Stats = ExplorationActor->GetUnitStats();
	if (!Stats) { return; }

	UGameInstance* GI = GetGameInstance();
	if (!GI) { return; }

	UPartyManagerSubsystem* PartyMgr = GI->GetSubsystem<UPartyManagerSubsystem>();
	if (!PartyMgr) { return; }

	// The exploration character represents the player's lead unit — conventionally
	// the first active member. Match by CharacterClass since the exploration actor
	// is not associated with a CharacterID directly.
	const TSubclassOf<ACharacter> ActorClass = ExplorationActor->GetClass();

	const FPartyData& Data = PartyMgr->GetPartyData();
	for (const FPartyMemberData& Member : Data.Members)
	{
		if (Member.CharacterClass != ActorClass) { continue; }

		// Apply the post-battle HP from FPartyMemberData to the freshly spawned actor.
		// This keeps the exploration actor's UnitStatsComponent in sync with the
		// authoritative party data so world saves serialize the correct value.
		const int32 SyncedHP = FMath::RoundToInt(Member.CurrentHP);
		Stats->SetCurrentHP(SyncedHP);

		UE_LOG(LogTemp, Log,
			TEXT("[GameContextManager] SyncExplorationActorHP: %s -> PersistentHP set to %d"),
			*GetNameSafe(ExplorationActor), SyncedHP);
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[GameContextManager] SyncExplorationActorHP: no matching FPartyMemberData for class %s"),
		*GetNameSafe(ActorClass));
}

ACharacterBase* UGameContextManager::SpawnExplorationActor()
{
	UWorld* World = GetWorld();
	if (!World) { return nullptr; }

	FTransform SpawnTransform = ActiveBattleSession.ExplorationCharacterTransform;
	if (SpawnTransform.GetTranslation().IsNearlyZero())
	{
		if (ActiveBattleSession.EncounterSource.IsValid())
		{
			SpawnTransform = ActiveBattleSession.EncounterSource->GetActorTransform();
			UE_LOG(LogTemp, Warning,
				TEXT("[GameContextManager] SpawnExplorationActor: transform zero — using EncounterVolume fallback."));
		}
	}
	
	UE_LOG(LogTemp, Log,
	TEXT("[GameContextManager] SpawnExplorationActorFromClass: SpawnTransform = %s"),
	*SpawnTransform.GetTranslation().ToString());

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ACharacterBase* NewCharacter = World->SpawnActor<ACharacterBase>(
		ActiveBattleSession.ExplorationCharacterClass,
		SpawnTransform,
		SpawnParams);

	if (!IsValid(NewCharacter))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GameContextManager] SpawnExplorationActor: spawn failed."));
		return nullptr;
	}

	SyncExplorationActorHP(NewCharacter);
	return NewCharacter;
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
		// Build SourceIndex -> actor map so WriteBackBattleResults can correlate
		// survivors correctly regardless of array ordering.
		// GeneratedParty[i] was spawned as SourceIndex=i; units absent from the
		// map (died or failed to spawn) will be written back with HP=0.
		TMap<int32, ACharacterBase*> AliveBySourceIndex;
		for (ACharacterBase* Ally : ActiveCombatManager->GetSpawnedAllies())
		{
			if (!IsValid(Ally)) { continue; }
			const int32 Idx = ActiveCombatManager->FindStableSourceIndexForUnit(Ally, EBattleUnitTeam::Ally);
			if (Idx != INDEX_NONE)
			{
				AliveBySourceIndex.Add(Idx, Ally);
			}
		}

		PartyManager->WriteBackBattleResults(
			AliveBySourceIndex,
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

TSubclassOf<ACharacterBase> UGameContextManager::GetExplorationCharacterClass() const
{
	return ActiveBattleSession.ExplorationCharacterClass;
}

FTransform UGameContextManager::GetSavedExplorationTransform() const
{
	return ActiveBattleSession.ExplorationCharacterTransform;
}

float UGameContextManager::GetSavedExplorationArmLength() const
{
	return ActiveBattleSession.ExplorationArmLength;
}

void UGameContextManager::ClearBattleSession()
{
	ActiveBattleSession.Reset();
}

FRotator UGameContextManager::GetSavedExplorationControlRotation() const
{
	return ActiveBattleSession.ExplorationControlRotation;
}

ACharacterBase* UGameContextManager::SpawnExplorationActorFromClass(
	TSubclassOf<ACharacterBase> CharacterClass)
{
	UWorld* World = GetWorld();
	if (!World || !CharacterClass) { return nullptr; }

	FTransform SpawnTransform = ActiveBattleSession.ExplorationCharacterTransform;

	// If transform is zero (new game — no save data yet), find the PlayerStart.
	// FTransform default-constructs to identity with translation (0,0,0), so
	// IsNearlyZero() on the translation is a reliable "not set" check.
	if (SpawnTransform.GetTranslation().IsNearlyZero())
	{
		if (AGameModeBase* GM = World->GetAuthGameMode())
		{
			if (AActor* StartSpot = GM->ChoosePlayerStart(nullptr))
			{
				SpawnTransform = StartSpot->GetActorTransform();

				UE_LOG(LogTemp, Log,
					TEXT("[GameContextManager] SpawnExplorationActorFromClass: "
						 "no saved transform — using PlayerStart '%s'."),
					*GetNameSafe(StartSpot));
			}
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ACharacterBase* NewCharacter = World->SpawnActor<ACharacterBase>(
		CharacterClass, SpawnTransform, SpawnParams);

	if (IsValid(NewCharacter))
	{
		ActiveBattleSession.ExplorationCharacter      = NewCharacter;
		ActiveBattleSession.ExplorationCharacterClass = CharacterClass;

		SyncExplorationActorHP(NewCharacter);

		UE_LOG(LogTemp, Log,
			TEXT("[GameContextManager] SpawnExplorationActorFromClass: spawned %s."),
			*GetNameSafe(NewCharacter));
	}
	else
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GameContextManager] SpawnExplorationActorFromClass: spawn failed for class %s."),
			*GetNameSafe(CharacterClass));
	}

	return NewCharacter;
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

TSubclassOf<ACharacterBase> UGameContextManager::ResolveExplorationPawnClass(
    TSubclassOf<ACharacterBase> FallbackClass) const
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) { return FallbackClass; }

    UPartyManagerSubsystem* PartyMgr = GI->GetSubsystem<UPartyManagerSubsystem>();
    if (!PartyMgr) { return FallbackClass; }

    const FPartyMemberData* Leader = PartyMgr->GetPartyLeader();
    if (!Leader) { return FallbackClass; }

    // Use the leader's dedicated exploration pawn if assigned,
    // otherwise fall back to the GameMode's prototype class.
    if (Leader->ExplorationPawnClass)
    {
        return TSubclassOf<ACharacterBase>(Leader->ExplorationPawnClass);
    }

    return FallbackClass;
}

void UGameContextManager::HandleLeaderChanged()
{
    // Only relevant during exploration — during combat the pawn is a battle unit
    if (CurrentState != EGameContextState::Exploration) { return; }

    ACharacterBase* OldPawn = ActiveBattleSession.ExplorationCharacter;
    if (!IsValid(OldPawn)) { return; }

    // Save the current position so the new leader spawns in the same spot
    ActiveBattleSession.ExplorationCharacterTransform = OldPawn->GetActorTransform();

    // Find the controller that currently possesses the old pawn
    APlayerController* PC = OldPawn->GetController<APlayerController>();

    // Destroy old pawn
    ActiveBattleSession.ExplorationCharacter = nullptr;
    if (PC) { PC->UnPossess(); }
    OldPawn->Destroy();

    // Resolve the new leader's pawn class
    UGameInstance* GI = GetGameInstance();
    if (!GI) { return; }

    ATowerFloorGameMode* GM = GetActiveFloorGameMode();
    TSubclassOf<ACharacterBase> FallbackClass =
        GM ? GM->GetExplorationCharacterClass() : nullptr;

    TSubclassOf<ACharacterBase> NewPawnClass = ResolveExplorationPawnClass(FallbackClass);
    if (!NewPawnClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[GameContextManager] HandleLeaderChanged: no pawn class available."));
        return;
    }

    ACharacterBase* NewPawn = SpawnExplorationActorFromClass(NewPawnClass);

    if (NewPawn && PC)
    {
        PC->Possess(NewPawn);

        UE_LOG(LogTemp, Log,
            TEXT("[GameContextManager] HandleLeaderChanged: new leader pawn possessed: %s"),
            *GetNameSafe(NewPawn));
    }
}

void UGameContextManager::InitializeSubsystem()
{
	if (UPartyManagerSubsystem* PartyMgr = GetGameInstance()->GetSubsystem<UPartyManagerSubsystem>())
	{
		PartyMgr->OnLeaderChanged.AddUObject(this, &UGameContextManager::HandleLeaderChanged);
	}
}

// ---------------------------------------------------------------------------
// Mid-combat save restore
// ---------------------------------------------------------------------------

void UGameContextManager::HandleSaveLoaded()
{
    UMyGameInstance* MyGI = Cast<UMyGameInstance>(GetGameInstance());
    if (!MyGI) { return; }

    UMySaveGame* Save = MyGI->GetActiveSave();
    if (!Save) { return; }

    // --- Mid-combat restore path ---
    if (Save->bSavedMidCombat)
    {
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
        return;
    }

    // --- Normal exploration load path ---
    // Read the player's saved transform from the actor save data and store it
    // in ActiveBattleSession so SpawnExplorationActor uses it instead of
    // spawning at PlayerStart. This must happen before RequestInitialState
    // (called by TowerFloorGameMode::BeginPlay) triggers pawn spawn.
    UWorld* World = GetWorld();
    if (!World) { return; }

    ATowerFloorGameMode* GM = GetActiveFloorGameMode();
    if (!GM) { return; }

    // Resolve the exploration character class from the GameMode
    if (GM->GetExplorationCharacterClass())
    {
        ActiveBattleSession.ExplorationCharacterClass = GM->GetExplorationCharacterClass();
    }

    // Find the player's saved transform by searching SavedActorsByLevel for
    // an entry whose class matches ExplorationCharacterClass
    if (ActiveBattleSession.ExplorationCharacterClass)
    {
    	if (UWorldManagerSubsystem* WorldMgr = World->GetSubsystem<UWorldManagerSubsystem>())
    	{
    		const FName LevelKey = WorldMgr->GetSanitizedLevelKey();
    		const TArray<FActorSaveData>& SavedActors = MyGI->GetSavedActorsForLevel(LevelKey);

    		for (const FActorSaveData& Data : SavedActors)
    		{
    			if (Data.ActorClass.IsNull()) { continue; }

    			UClass* SavedClass = Data.ActorClass.LoadSynchronous();
    			if (SavedClass && SavedClass->IsChildOf(ActiveBattleSession.ExplorationCharacterClass))
    			{
    				ActiveBattleSession.ExplorationCharacterTransform = Data.Transform;

    				UE_LOG(LogTemp, Log,
						TEXT("[GameContextManager] HandleSaveLoaded: restored player transform from save data."));
    				UE_LOG(LogTemp, Log,
						TEXT("[GameContextManager] HandleSaveLoaded: searching level key '%s', found %d saved actors."),
						*LevelKey.ToString(), SavedActors.Num());
    				break;
    			}
    		}
    	}
    }
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

    // Restore exploration character data from the saved combat session so
    // ReturnToExploration() can respawn the player pawn after combat ends.
    // On a normal flow this data lives in FBattleSessionData (populated by
    // RequestBattlePreparation); on a restore there is no exploration character
    // in memory, so we serialized it into FSavedCombatSession at save time.
    if (SavedSession.ExplorationCharacterClass.IsValid())
    {
        ActiveBattleSession.ExplorationCharacterClass     = SavedSession.ExplorationCharacterClass.LoadSynchronous();
        ActiveBattleSession.ExplorationCharacterTransform = SavedSession.ExplorationCharacterTransform;
        ActiveBattleSession.ExplorationControlRotation    = SavedSession.ExplorationControlRotation;
        ActiveBattleSession.ExplorationArmLength          = SavedSession.ExplorationArmLength;
        UE_LOG(LogTemp, Log,
            TEXT("[GameContextManager] RequestBattleCombatRestore: exploration class restored — %s"),
            *GetNameSafe(ActiveBattleSession.ExplorationCharacterClass));
    }
    else
    {
        // Fallback for saves created before this field was serialized.
        // Use the GameMode's DefaultPawnClass if set.
        if (ATowerFloorGameMode* GM = GetActiveFloorGameMode())
        {
            if (GM->DefaultPawnClass && GM->DefaultPawnClass->IsChildOf(ACharacterBase::StaticClass()))
            {
                ActiveBattleSession.ExplorationCharacterClass = GM->DefaultPawnClass;
                UE_LOG(LogTemp, Warning,
                    TEXT("[GameContextManager] RequestBattleCombatRestore: exploration class missing in save — falling back to DefaultPawnClass (%s). Set DefaultPawnClass in BP_TowerFloorGameMode."),
                    *GetNameSafe(ActiveBattleSession.ExplorationCharacterClass));
            }
            else
            {
                UE_LOG(LogTemp, Error,
                    TEXT("[GameContextManager] RequestBattleCombatRestore: no exploration class available — ReturnToExploration will fail. Set DefaultPawnClass in BP_TowerFloorGameMode."));
            }
        }
    }

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

    // --- 6. Start combat using the save-aware initialization path ---
    // NOTE: RestoreCombatFromSave is called inside StartCombatFromSave (after units
    // are spawned). Calling it here would be a no-op because SpawnedAllies/Enemies
    // are not yet populated. The single call inside StartCombatFromSave is authoritative.
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