// Copyright Xyzto Works


#include "GameMode/CombatManager.h"

#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/SaveableComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Audio/AudioSubsystem.h"
#include "Characters/Abilities/BattleMovementAbility.h"
#include "Engine/Texture2D.h"
#include "GameInstance/MyGameInstance.h"
#include "GameInstance/GameContextManager.h"
#include "World/WorldManagerSubsystem.h"
#include "GameMode/BattlePreparationContext.h"
#include "GameMode/Combat/BattleInputManager.h"
#include "GameMode/Combat/BattleCameraPawn.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "GameMode/Combat/StatusEffectManager.h"
#include "GameMode/Combat/AI/BattleTeamAIPlanner.h"
#include "Characters/CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Characters/PlayerControllers/BattlePlayerController.h"
#include "Data/StatusEffectDefinition.h"
#include "GameMode/TurnManager.h"
#include "GameMode/Combat/BattleInputContextBuilder.h"
#include "Level/FloorEncounterVolume.h"
#include "Level/GridManager.h"
#include "Types/BattleTypes.h"


// ---------------------------------------------------------------------------
// Spawn Z helper
// ---------------------------------------------------------------------------

// Returns the correct world Z for spawning a character of the given class on a tile.
//
// SpawnActor positions the actor with its ORIGIN (capsule centre) at the supplied
// transform location. GetTileCenterWorld returns the ground surface Z from the
// line-trace hit. Passing that value directly as origin embeds the bottom half of
// the capsule below the surface; the collision adjustment then pushes the actor
// upward, leaving it floating one half-height above the tile.
//
// The fix is identical to what GA_Move_Simple does via GetGroundAlignedLocation:
// offset Z by the capsule half-height so the capsule base sits on the surface.
// We read the value from the CDO (Class Default Object) — the engine-managed
// template instance that always exists without requiring a live spawned actor.
static FVector GetSpawnLocationForClass(UClass* ActorClass, const FVector& TileCenter)
{
    if (!ActorClass)
    {
        return TileCenter;
    }

    const ACharacterBase* CDO = Cast<ACharacterBase>(ActorClass->GetDefaultObject());
    if (!CDO)
    {
        return TileCenter;
    }

    const UCapsuleComponent* Capsule = CDO->GetCapsuleComponent();
    const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f;

    return TileCenter + FVector(0.f, 0.f, HalfHeight);
}

void UCombatManager::Initialize()
{
	// Setup participants
}

void UCombatManager::RegisterStableSourceIndex(ACharacterBase* Unit, EBattleUnitTeam Team, int32 SourceIndex)
{
	if (!IsValid(Unit) || SourceIndex == INDEX_NONE)
	{
		return;
	}

	switch (Team)
	{
	case EBattleUnitTeam::Ally:
		AllySourceIndices.Add(Unit, SourceIndex);
		break;

	case EBattleUnitTeam::Enemy:
		EnemySourceIndices.Add(Unit, SourceIndex);
		break;

	default:
		break;
	}
}

int32 UCombatManager::FindStableSourceIndexForUnit(ACharacterBase* Unit, EBattleUnitTeam Team) const
{
	if (!IsValid(Unit))
	{
		return INDEX_NONE;
	}

	const TMap<TWeakObjectPtr<ACharacterBase>, int32>* MapPtr = nullptr;

	switch (Team)
	{
	case EBattleUnitTeam::Ally:
		MapPtr = &AllySourceIndices;
		break;

	case EBattleUnitTeam::Enemy:
		MapPtr = &EnemySourceIndices;
		break;

	default:
		return INDEX_NONE;
	}

	if (!MapPtr)
	{
		return INDEX_NONE;
	}

	if (const int32* Found = MapPtr->Find(Unit))
	{
		return *Found;
	}

	return INDEX_NONE;
}

void UCombatManager::ClearStableSourceIndices()
{
	AllySourceIndices.Reset();
	EnemySourceIndices.Reset();
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
		FinalSpawnTransform.SetLocation(
			GetSpawnLocationForClass(Spawn.ActorClass, Grid->GetTileCenterWorld(SpawnCoord)));

		// AdjustIfPossibleButAlwaysSpawn: if two planned spawns land on adjacent
		// tiles whose capsules slightly overlap, the spawn still succeeds instead
		// of silently failing and dropping the unit from the battle.
		FActorSpawnParameters AllySpawnParams;
		AllySpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AActor* SpawnedAlly = GetWorld()->SpawnActor<AActor>(
			Spawn.ActorClass,
			FinalSpawnTransform,
			AllySpawnParams
		);

		ACharacterBase* Character = Cast<ACharacterBase>(SpawnedAlly);
		if (!Character)
		{
			UE_LOG(LogTemp, Error,
				TEXT("[CombatManager] SpawnAllies: SpawnActor returned null for index %d (class: %s)"),
				i, *GetNameSafe(Spawn.ActorClass));
			continue;
		}

		// Register in the OccupancyMap immediately so the next iteration's
		// FindClosestValidTileToWorld sees this tile as occupied and won't
		// assign two units to the same or physically overlapping tile.
		Grid->UpdateActorPosition(Character, SpawnCoord);

		BindToCombatUnitActionEvents(Character);
		SpawnedAllies.Add(Character);

		// Inject the event bus so this unit's abilities can route through it
		if (UBattleModeComponent* BattleComp = Character->GetBattleModeComponent())
		{
			BattleComp->SetCombatEventBus(EventBus);
			BattleComp->SetTurnManager(TurnManager);
		}

		FSpawnedBattleUnit Entry;
		Entry.UnitActor = Character;
		Entry.SourceIndex = i;
		Entry.Team = EBattleUnitTeam::Ally;
		
		RegisterStableSourceIndex(Character, EBattleUnitTeam::Ally, i);
		
		const FGeneratedPlayerData& PlayerData = BattlePrepContext.PlayerParty[i];
		if (UUnitStatsComponent* Stats = Character->GetUnitStats())
		{
			// Apply the member's persistent level before any HP or stat calculation.
			// BattleState::Initialize calls GetMaxHP() which depends on Stats->Level,
			// so this must happen before InitializeForBattle and before BattleState runs.
			Stats->Level = PlayerData.Level;

			if (PlayerData.PersistentHP > 0)
			{
				Stats->SetCurrentHP(PlayerData.PersistentHP);
				UE_LOG(LogTemp, Log,
					TEXT("[CombatManager] Restored PersistentHP=%d for %s (Lv%d)"),
					PlayerData.PersistentHP, *GetNameSafe(Character), Stats->Level);
			}
			Stats->InitializeForBattle();
		}

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
            UE_LOG(LogTemp, Warning,
                TEXT("[CombatManager]: Failed to find valid enemy spawn tile for index %d"), i);
            continue;
        }

        FTransform FinalSpawnTransform = Spawn.PlannedTransform;
        FinalSpawnTransform.SetLocation(
            GetSpawnLocationForClass(Spawn.ActorClass, Grid->GetTileCenterWorld(SpawnCoord)));

        FActorSpawnParameters EnemySpawnParams;
        EnemySpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        AActor* SpawnedEnemy = GetWorld()->SpawnActor<AActor>(
            Spawn.ActorClass,
            FinalSpawnTransform,
            EnemySpawnParams
        );

        ACharacterBase* Character = Cast<ACharacterBase>(SpawnedEnemy);
        if (!Character)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[CombatManager] SpawnEnemies: SpawnActor returned null for index %d (class: %s)"),
                i, *GetNameSafe(Spawn.ActorClass));
            continue;
        }

        Grid->UpdateActorPosition(Character, SpawnCoord);
        BindToCombatUnitActionEvents(Character);
        SpawnedEnemies.Add(Character);

        // Inject the event bus so this unit's abilities can route through it
        if (UBattleModeComponent* BattleComp = Character->GetBattleModeComponent())
        {
            BattleComp->SetCombatEventBus(EventBus);
        }

        // Apply level from EnemyParty data before BattleState reads GetMaxHP().
        // EnemyPlannedSpawns[i] corresponds to EnemyParty[i] — same index, same unit.
        // Without this, GetMaxHP() uses the CDO's default level, ignoring the
        // level set in the encounter data.
        if (UUnitStatsComponent* Stats = Character->GetUnitStats())
        {
            if (BattlePrepContext.EnemyParty.IsValidIndex(i))
            {
                Stats->Level = BattlePrepContext.EnemyParty[i].Level;

                UE_LOG(LogTemp, Log,
                    TEXT("[CombatManager] SpawnEnemies: %s spawned at Lv%d"),
                    *GetNameSafe(Character), Stats->Level);
            }

            Stats->InitializeForBattle();
        }

        FSpawnedBattleUnit Entry;
        Entry.UnitActor   = Character;
        Entry.SourceIndex = i;
        Entry.Team        = EBattleUnitTeam::Enemy;

        RegisterStableSourceIndex(Character, EBattleUnitTeam::Enemy, i);
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

    // Store the encounter volume reference so CollectCombatSaveData can record its guid.
    ActiveEncounterVolume = BattlePrepContext.EncounterSource;
	
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
	EventBus->OnStatusApplied.AddUObject(this, &UCombatManager::HandleStatusApplied);
	
	// Bind the AudioSubsystem to this battle's event bus so combat SFX
	// (hits, deaths, heals) fire automatically for the duration of this session
	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
		{
			Audio->BindToCombatEventBus(EventBus);
			UE_LOG(LogTemp, Log, TEXT("[CombatManager] AudioSubsystem bound to CombatEventBus."));
		}
	}

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
	BattleTeamAIPlanner->OnAIActionFinished.AddUObject(this, &UCombatManager::HandleAIActionFinished);

	// Create input manager
	BattleInputManager = NewObject<UBattleInputManager>(this);
	BattleInputManager->Initialize(TurnManager, BattleCameraPawn, this);
	
	// Create input context builder
	InputContextBuilder = NewObject<UBattleInputContextBuilder>(this);
	InputContextBuilder->SetTurnManager(TurnManager);
	UpdateInputContext();

	// BattleState is initialized here
	BattleState = NewObject<UBattleState>();
	BattleState->Initialize(BattlePrepContext);

	// Wire BattleState
	EventBus->Initialize(BattleState);
	BattleTeamAIPlanner->SetBattleState(BattleState);

	// Create and initialize StatusEffectManager — must happen after BattleState
	// and TurnManager exist, and before units' BattleModeComponents are finalized
	StatusEffectManager = NewObject<UStatusEffectManager>(this);
	StatusEffectManager->Initialize(BattleState, EventBus, TurnManager);
	
	EventBus->SetStatusEffectManager(StatusEffectManager);
	
	for (ACharacterBase* Unit : SpawnedAllies)
	{
		if (UBattleModeComponent* BattleComp = Unit ? Unit->GetBattleModeComponent() : nullptr)
		{
			BattleComp->SetBattleState(BattleState);
			BattleComp->SetStatusEffectManager(StatusEffectManager);
		}
	}
	for (ACharacterBase* Unit : SpawnedEnemies)
	{
		if (UBattleModeComponent* BattleComp = Unit ? Unit->GetBattleModeComponent() : nullptr)
		{
			BattleComp->SetBattleState(BattleState);
			BattleComp->SetStatusEffectManager(StatusEffectManager);
		}
	}

	// Inject TurnManager into every unit's BattleModeComponent.
	// This must happen after TurnManager is created (line above SpawnAllies ran
	// with TurnManager still null, so SetTurnManager got nullptr then).
	for (ACharacterBase* Unit : SpawnedAllies)
	{
		if (UBattleModeComponent* BattleComp = Unit ? Unit->GetBattleModeComponent() : nullptr)
		{
			BattleComp->SetTurnManager(TurnManager);
		}
	}
	for (ACharacterBase* Unit : SpawnedEnemies)
	{
		if (UBattleModeComponent* BattleComp = Unit ? Unit->GetBattleModeComponent() : nullptr)
		{
			BattleComp->SetTurnManager(TurnManager);
		}
	}

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

void UCombatManager::StartCombatFromSave(
	UBattlePreparationContext& BattlePrepContext,
	const FSavedCombatSession& SavedSession
)
{
	UE_LOG(LogTemp, Warning, TEXT("=== StartCombatFromSave ==="));

	ActiveEncounterVolume = BattlePrepContext.EncounterSource;

	UGridManager* Grid = GetWorld()->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatManager] StartCombatFromSave: Grid is invalid"));
		return;
	}

	// Reset runtime arrays before restoring.
	SpawnedAllies.Reset();
	SpawnedEnemies.Reset();
	BattlePrepContext.SpawnedUnits.Reset();
	ClearStableSourceIndices();

	// Identical wiring to StartCombat — EventBus first.
	EventBus = NewObject<UCombatEventBus>(this);
	EventBus->OnUnitDied.AddUObject(this, &UCombatManager::HandleUnitDeath);
	EventBus->OnStatusApplied.AddUObject(this, &UCombatManager::HandleStatusApplied);

	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
		{
			Audio->BindToCombatEventBus(EventBus);
		}
	}

	// Build lookup tables by team + original source index.
	TMap<int32, const FSavedCombatUnitState*> SavedAlliesByIndex;
	TMap<int32, const FSavedCombatUnitState*> SavedEnemiesByIndex;

	for (const FSavedCombatUnitState& SavedUnit : SavedSession.UnitStates)
	{
		if (SavedUnit.SourceIndex == INDEX_NONE)
		{
			continue;
		}

		if (SavedUnit.Team == EBattleUnitTeam::Ally)
		{
			SavedAlliesByIndex.Add(SavedUnit.SourceIndex, &SavedUnit);
		}
		else if (SavedUnit.Team == EBattleUnitTeam::Enemy)
		{
			SavedEnemiesByIndex.Add(SavedUnit.SourceIndex, &SavedUnit);
		}
	}

	auto SpawnSavedTeamUnits =
		[&](const TArray<FPlannedSpawn>& PlannedSpawns,
			const TMap<int32, const FSavedCombatUnitState*>& SavedByIndex,
			EBattleUnitTeam Team)
	{
		for (int32 i = 0; i < PlannedSpawns.Num(); ++i)
		{
			const FSavedCombatUnitState* SavedUnit = SavedByIndex.FindRef(i);
			if (!SavedUnit)
			{
				// Nothing saved for this original slot.
				continue;
			}

			// Dead units do not come back.
			if (SavedUnit->bWasDead || SavedUnit->CurrentHP <= 0)
			{
				continue;
			}

			const FPlannedSpawn& PlannedSpawn = PlannedSpawns[i];

			FIntPoint SpawnCoord;
			bool bHasValidSavedCoord = false;

			if (SavedUnit->bHasSavedGridCoord && Grid->GetTile(SavedUnit->SavedGridCoord))
			{
				SpawnCoord = SavedUnit->SavedGridCoord;
				bHasValidSavedCoord = true;
			}

			if (!bHasValidSavedCoord)
			{
				if (!Grid->FindClosestValidTileToWorld(
					PlannedSpawn.PlannedTransform.GetLocation(),
					SpawnCoord,
					true))
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT("[CombatManager] StartCombatFromSave: failed to find fallback spawn tile for %s index %d"),
						Team == EBattleUnitTeam::Ally ? TEXT("ally") : TEXT("enemy"),
						i
					);
					continue;
				}
			}

			// IMPORTANT:
			// Restore the exact saved class, NOT the freshly planned class.
			UClass* ClassToSpawn = SavedUnit->ActorClass.LoadSynchronous();
			if (!ClassToSpawn)
			{
				ClassToSpawn = PlannedSpawn.ActorClass;
			}

			if (!ClassToSpawn)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[CombatManager] StartCombatFromSave: no valid class for %s index %d"),
					Team == EBattleUnitTeam::Ally ? TEXT("ally") : TEXT("enemy"),
					i
				);
				continue;
			}

			FTransform FinalSpawnTransform = PlannedSpawn.PlannedTransform;
			FinalSpawnTransform.SetLocation(
				GetSpawnLocationForClass(ClassToSpawn, Grid->GetTileCenterWorld(SpawnCoord)));

			// Use AdjustIfPossibleButAlwaysSpawn so units are never silently dropped
			// due to residual physics overlap from actors destroyed earlier this frame.
			// Without this, SpawnActor returns null on collision and the unit is lost.
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(
				ClassToSpawn,
				FinalSpawnTransform,
				SpawnParams
			);

			ACharacterBase* Character = Cast<ACharacterBase>(SpawnedActor);
			if (!Character)
			{
				UE_LOG(LogTemp, Error,
					TEXT("[CombatManager] StartCombatFromSave: SpawnActor returned null for %s index %d — class: %s"),
					(Team == EBattleUnitTeam::Ally ? TEXT("ally") : TEXT("enemy")),
					i,
					*GetNameSafe(ClassToSpawn));
				continue;
			}

			// Reassign the saved GUID immediately so restore matches correctly.
			if (USaveableComponent* SaveComp = Character->FindComponentByClass<USaveableComponent>())
			{
				SaveComp->SetActorGuid(SavedUnit->ActorGuid);
			}

			BindToCombatUnitActionEvents(Character);

			if (Team == EBattleUnitTeam::Ally)
			{
				SpawnedAllies.Add(Character);
			}
			else
			{
				SpawnedEnemies.Add(Character);
			}

			if (UBattleModeComponent* BattleComp = Character->GetBattleModeComponent())
			{
				BattleComp->SetCombatEventBus(EventBus);
				BattleComp->SetTurnManager(TurnManager);
			}

			InitializeSpawnedUnitForBattle(Character, Team, Grid);

			// Exact tactical position from save.
			Grid->UpdateActorPosition(Character, SpawnCoord);

			FSpawnedBattleUnit Entry;
			Entry.UnitActor = Character;
			Entry.SourceIndex = i;
			Entry.Team = Team;
			BattlePrepContext.SpawnedUnits.Add(Entry);

			RegisterStableSourceIndex(Character, Team, i);

			UE_LOG(
				LogTemp,
				Log,
				TEXT("[CombatManager] Restored %s at tile (%d, %d) | SourceIndex=%d"),
				*GetNameSafe(Character),
				SpawnCoord.X,
				SpawnCoord.Y,
				i
			);
		}
	};

	SpawnSavedTeamUnits(
		BattlePrepContext.PlayerPlannedSpawns,
		SavedAlliesByIndex,
		EBattleUnitTeam::Ally
	);

	SpawnSavedTeamUnits(
		BattlePrepContext.EnemyPlannedSpawns,
		SavedEnemiesByIndex,
		EBattleUnitTeam::Enemy
	);

	TArray<AActor*> AllCombatants;
	AllCombatants.Append(SpawnedAllies);
	AllCombatants.Append(SpawnedEnemies);

	TurnManager = NewObject<UTurnManager>(this);
	TurnManager->Initialize(AllCombatants);

	BattleTeamAIPlanner = NewObject<UBattleTeamAIPlanner>(this);
	BattleTeamAIPlanner->Initialize(AllCombatants);
	BattleTeamAIPlanner->OnAIActionFinished.AddUObject(this, &UCombatManager::HandleAIActionFinished);

	BattleInputManager = NewObject<UBattleInputManager>(this);
	BattleInputManager->Initialize(TurnManager, BattleCameraPawn, this);

	InputContextBuilder = NewObject<UBattleInputContextBuilder>(this);
	InputContextBuilder->SetTurnManager(TurnManager);
	UpdateInputContext();

	BattleState = NewObject<UBattleState>();
	BattleState->InitializeFromSave(BattlePrepContext, SavedSession);

	EventBus->Initialize(BattleState);
	BattleTeamAIPlanner->SetBattleState(BattleState);

	StatusEffectManager = NewObject<UStatusEffectManager>(this);
	StatusEffectManager->Initialize(BattleState, EventBus, TurnManager);
	EventBus->SetStatusEffectManager(StatusEffectManager);

	for (ACharacterBase* Unit : SpawnedAllies)
	{
		if (UBattleModeComponent* BM = Unit ? Unit->GetBattleModeComponent() : nullptr)
		{
			BM->SetBattleState(BattleState);
			BM->SetStatusEffectManager(StatusEffectManager);
			BM->SetTurnManager(TurnManager);
		}
	}

	for (ACharacterBase* Unit : SpawnedEnemies)
	{
		if (UBattleModeComponent* BM = Unit ? Unit->GetBattleModeComponent() : nullptr)
		{
			BM->SetBattleState(BattleState);
			BM->SetStatusEffectManager(StatusEffectManager);
			BM->SetTurnManager(TurnManager);
		}
	}

	RestoreCombatFromSave(SavedSession);

	TurnManager->OnTurnStateChanged.AddUObject(
		this,
		&UCombatManager::HandleTurnStateChanged
	);

	// Save is taken at end of enemy turn, so restore resumes on player turn.
	TurnManager->StartCombat();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[CombatManager] StartCombatFromSave: combat restored and running")
	);
}

void UCombatManager::InitializeSpawnedUnitForBattle(ACharacterBase* Character, EBattleUnitTeam Team, UGridManager* Grid)
{
	if (!Character || !Grid)
	{
		return;
	}

	Grid->RegisterActorToGrid(Character);

	// Assign team identity — this is persistent data, not battle session data
	if (UUnitStatsComponent* UnitStats = Character->GetUnitStats())
	{
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
	
	
	UE_LOG(LogTemp, Error, TEXT("[HandleUnitDeath] Called for %s"), *GetNameSafe(DeadUnit));
	
	// Hide the mesh and disable collision immediately.
	// The actor stays alive in memory — all units are destroyed together in Cleanup()
	// at the end of combat to avoid mid-battle destruction hitches.
	DeadUnit->PlayDeathVisual();

	// Remove from grid occupancy so the tile is freed for pathing and spawning.
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
	case EBattleUnitTeam::Ally:  TeamUnits = &SpawnedAllies;  break;
	case EBattleUnitTeam::Enemy: TeamUnits = &SpawnedEnemies; break;
	default: return false;
	}

	for (ACharacterBase* Unit : *TeamUnits)
	{
		if (!Unit) { continue; }

		// Read alive state from BattleUnitState — the authority during combat
		if (BattleState)
		{
			const FBattleUnitState* State = BattleState->FindUnitState(Unit);
			if (State && State->IsAlive()) { return false; }
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

	BattleState->ResetTurnStateForTeam(EBattleUnitTeam::Ally);
	
	for (ACharacterBase* Enemy : SpawnedEnemies)
	{
		BindToCombatUnitActionEvents(Enemy);
	}

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
	OnEnemyTurnBegan.Broadcast();

	BattleState->ResetTurnStateForTeam(EBattleUnitTeam::Enemy);
	
	for (ACharacterBase* Enemy : SpawnedEnemies)
	{
		if (UBattleModeComponent* BattleComp = Enemy
			? Enemy->FindComponentByClass<UBattleModeComponent>()
			: nullptr)
		{
			// Keep Started (camera lock) — remove only Finished (recursion source)
			BattleComp->OnActionUseFinished.RemoveDynamic(
				this, &UCombatManager::HandleUnitActionFinished);
		}
	}

	InputContextBuilder->SetCameraInputEnabled(false);
	InputContextBuilder->SetHardLock(true);
	UpdateInputContext();

	BattleTeamAIPlanner->StartTeamTurn();
}

void UCombatManager::SelectFirstAvailableUnit()
{
	UE_LOG(LogTemp, Warning, TEXT("[CombatManager]: Trying to select first available unit..."));
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
	OnActiveUnitChanged.Broadcast(CurrentSelectedUnit);

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

void UCombatManager::HandleStatusApplied(ACharacterBase* Target, const FGameplayTag& StatusTag, UTexture2D* /*Icon*/)
{
	// This fires whenever any unit receives a status effect.
	// We only care if it affected the currently active player unit — e.g. an enemy ability
	// applies Stun to the player's selected unit mid-turn. In that case we need to
	// immediately advance selection so the player doesn't try to act with a stunned unit.
	if (bCombatEnding || !BattleState || !TurnManager) { return; }
	if (TurnManager->GetCurrentTurnOwner() != EBattleTurnOwner::Player) { return; }
	if (Target != CurrentSelectedUnit) { return; }

	if (!BattleState->CanUnitAct(Target))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CombatManager] Status '%s' applied to active unit %s — unit can no longer act, advancing selection"),
			*StatusTag.ToString(), *GetNameSafe(Target));

		SelectNextControllableUnit();
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

	// If a post-action delay is in progress, suppress the unit switch here.
	// The delay callback will call SelectNextControllableUnit once it fires,
	// ensuring the camera only moves after the full delay has elapsed.
	if (GetWorld()->GetTimerManager().IsTimerActive(PostActionDelayHandle) || bWaitingForPathFinish)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatManager] OutOfAP suppressed — waiting for post-action delay or path finish."));
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

    // Mid-combat auto-save at end of enemy turn.
    // This must be equivalent to AutoSaveBeforeTravel: it collects world actor
    // state (encounter volumes, interactables, global flags) AND the combat
    // session snapshot so a reload restores the world exactly as left.
    if (UWorld* W = GetWorld())
    {
        if (UMyGameInstance* MyGI = Cast<UMyGameInstance>(W->GetGameInstance()))
        {
            if (UMySaveGame* Save = MyGI->GetActiveSave())
            {
                Save->LastSavedAt     = FDateTime::UtcNow();
                Save->bSavedMidCombat = true;

                // 1. Collect world actor state (encounter volumes, interactables, etc.).
                //    This is the same step AutoSaveBeforeTravel performs via
                //    WorldMgr->CollectWorldSaveData(), ensuring level objects are
                //    serialized even though we are mid-combat.
                if (UWorldManagerSubsystem* WorldMgr = W->GetSubsystem<UWorldManagerSubsystem>())
                {
                    WorldMgr->CollectWorldSaveData();
                }

                // 2. Collect combat session snapshot (unit HP, AP, statuses, cooldowns).
                CollectCombatSaveData(Save->SavedCombatSession);

                // 3. Persist exploration character data so ReturnToExploration() can
                //    respawn the player pawn after combat ends on a restore path.
                //    On a normal flow, FBattleSessionData holds this in memory;
                //    on a restore, that struct is empty so we must save it explicitly.
                if (UGameContextManager* CTX = MyGI->GetGameContextManager())
                {
                    Save->SavedCombatSession.ExplorationCharacterClass     = CTX->GetExplorationCharacterClass();
                    Save->SavedCombatSession.ExplorationCharacterTransform = CTX->GetSavedExplorationTransform();
                    Save->SavedCombatSession.ExplorationControlRotation    = CTX->GetSavedExplorationControlRotation();
                    Save->SavedCombatSession.ExplorationArmLength          = CTX->GetSavedExplorationArmLength();
                }

                // 4. Flush in-memory party/progression/flags into the save object.
                MyGI->CommitSave();

                UE_LOG(LogTemp, Log,
                    TEXT("[CombatManager] EndEnemyTurn: full mid-combat save collected — committing to disk"));
            }

            MyGI->RequestSaveGame();
        }
    }
	
	TurnManager->EndCurrentTurn();
}

void UCombatManager::RequestEndEnemyTurn()
{
	EndEnemyTurn();
}

void UCombatManager::CancelPreparation(TWeakObjectPtr<class AFloorEncounterVolume> EncounterSource)
{
	if (EncounterSource.IsValid())
	{
		EncounterSource->RestoreEncounterVisual();	
	}	
	
	OnCombatFinished.Broadcast(EBattleUnitTeam::None);
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
	OnActiveUnitChanged.Broadcast(nullptr);
	SelectedUnit = INDEX_NONE;
}

void UCombatManager::EndCombatWithWinner(EBattleUnitTeam WinningTeam)
{
	if (bCombatEnding) { return; }
	bCombatEnding = true;

	if (InputContextBuilder)
	{
		InputContextBuilder->SetCameraInputEnabled(false);
		InputContextBuilder->SetUnitInputEnabled(false);
	}

	// Persist HP and any other battle outcomes back to UnitStatsComponents
	// before actors are destroyed in Cleanup()
	if (BattleState)
	{
		BattleState->GenerateResult();
	}

    // Clear the mid-combat save flag so a subsequent load does not re-enter
    // combat after the battle has been resolved.
    if (UWorld* W = GetWorld())
    {
        if (UMyGameInstance* MyGI = Cast<UMyGameInstance>(W->GetGameInstance()))
        {
            if (UMySaveGame* Save = MyGI->GetActiveSave())
            {
                Save->bSavedMidCombat = false;
                Save->SavedCombatSession.Reset();
            }
        }
    }

	OnCombatFinished.Broadcast(WinningTeam);
}

int32 UCombatManager::GetAliveAllies() const
{
	int32 Count = 0;
	for (ACharacterBase* Ally : SpawnedAllies)
	{
		if (!IsValid(Ally) || !BattleState) { continue; }
		const FBattleUnitState* State = BattleState->FindUnitState(Ally);
		if (State && State->IsAlive()) { ++Count; }
	}
	return Count;
}

int32 UCombatManager::GetAliveEnemies() const
{
	int32 Count = 0;
	for (ACharacterBase* Enemy : SpawnedEnemies)
	{
		if (!IsValid(Enemy) || !BattleState) { continue; }
		const FBattleUnitState* State = BattleState->FindUnitState(Enemy);
		if (State && State->IsAlive()) { ++Count; }
	}
	return Count;
}

void UCombatManager::Cleanup()
{
	bCombatEnding = true;
	
	// Cancel any pending post-action delay to avoid dangling callbacks
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PostActionDelayHandle);
		GetWorld()->GetTimerManager().ClearTimer(AICameraWaitHandle);
		bWaitingForPathFinish = false;

	}

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

	if (StatusEffectManager)
	{
		StatusEffectManager->Shutdown();
		StatusEffectManager = nullptr;
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
	ClearStableSourceIndices();

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

    InputContextBuilder->SetUnitInputEnabled(false);
    InputContextBuilder->SetCameraInputEnabled(false);
    InputContextBuilder->SetHardLock(true);
    UpdateInputContext();

    UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Action finished -> updating input state"));

    // Check if this was a movement ability. If so, wait for the physical path to finish
    // before restoring input. OnActionUseFinished fires before the character reaches
    // the destination, so using a timer here would restore input too early (or not at all
    // with delay 0). Instead we bind once to OnPathMoveFinished on the acting unit.
    const bool bWasMovement = [&]() -> bool
    {
        if (const UBattleModeComponent* BattleComp = ActingUnit ? ActingUnit->GetBattleModeComponent() : nullptr)
        {
            const TSubclassOf<UGameplayAbility> UsedClass = BattleComp->GetCurrentContext().AbilityClass;
            return UsedClass && UsedClass->IsChildOf(UBattleMovementAbility::StaticClass());
        }
        return false;
    }();

    // For movement, use a minimal delay (one frame) so the timer fires after
    // the current Broadcast call stack unwinds. OnPathMoveFinished triggers
    // FinalizeAbilityExecution which synchronously calls us — binding to
    // OnPathMoveFinished here would arrive after the event already fired.
    const float Delay = bWasMovement ? 0.05f : 1.0f;
    if (bWasMovement) { bWaitingForPathFinish = true; }

    GetWorld()->GetTimerManager().SetTimer(
        PostActionDelayHandle,
        [this, ActingUnit]()
        {
            bWaitingForPathFinish = false;
            RestoreInputAfterAction(ActingUnit);
        }, Delay, false);
}

void UCombatManager::RestoreInputAfterAction(ACharacterBase* ActingUnit)
{
    if (bCombatEnding || !InputContextBuilder || !TurnManager)
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[CombatManager] Restoring input after action"));

    if (TurnManager->GetCurrentTurnOwner() == EBattleTurnOwner::Player)
    {
        InputContextBuilder->SetUnitInputEnabled(true);
        InputContextBuilder->SetCameraInputEnabled(true);
        InputContextBuilder->SetHardLock(false);

        if (ActingUnit && BattleState && !BattleState->CanUnitAct(ActingUnit))
        {
            SelectNextControllableUnit();
        }
        else if (ActingUnit && BattleState && BattleState->CanUnitAct(ActingUnit))
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

void UCombatManager::HandleAIActionFinished()
{
	if (bCombatEnding || !BattleTeamAIPlanner) { return; }

	if (BattleCameraPawn && BattleCameraPawn->HasLockedActor() && !BattleCameraPawn->IsNearLockedActor())
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatManager] AI action finished — waiting for camera to reach actor"));
		GetWorld()->GetTimerManager().SetTimer(
			AICameraWaitHandle,
			this,
			&UCombatManager::PollCameraReachedActor,
			0.05f,
			true
		);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatManager] AI action finished — deferring next action to next frame"));
		GetWorld()->GetTimerManager().SetTimer(
			AICameraWaitHandle,       // reuse the same handle — it's idle here
			[this]()
			{
				if (!bCombatEnding && BattleTeamAIPlanner)
				{
					BattleTeamAIPlanner->ContinueAfterActionDelay();
				}
			},
			0.05f,
			false                     // false = fires once, not looping
		);
	}
}

void UCombatManager::PollCameraReachedActor()
{
    if (!BattleCameraPawn || !BattleTeamAIPlanner)
    {
        GetWorld()->GetTimerManager().ClearTimer(AICameraWaitHandle);
        return;
    }

    if (BattleCameraPawn->IsNearLockedActor())
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatManager] Camera reached actor — continuing AI sequence"));
        GetWorld()->GetTimerManager().ClearTimer(AICameraWaitHandle);
        BattleTeamAIPlanner->ContinueAfterActionDelay();
    }
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
// ---------------------------------------------------------------------------
// Mid-combat save / restore
// ---------------------------------------------------------------------------

void UCombatManager::CollectCombatSaveData(FSavedCombatSession& OutSession) const
{
	OutSession.UnitStates.Reset();

	// Record the EncounterVolume guid so GameContextManager can find it on restore.
	if (ActiveEncounterVolume)
	{
		if (USaveableComponent* SaveComp =
			ActiveEncounterVolume->FindComponentByClass<USaveableComponent>())
		{
			OutSession.EncounterVolumeGuid = SaveComp->GetOrCreateGuid();
		}
	}

	UGridManager* Grid = GetWorld() ? GetWorld()->GetSubsystem<UGridManager>() : nullptr;

	auto CollectUnits = [&](const TArray<ACharacterBase*>& Units, EBattleUnitTeam Team)
	{
		for (ACharacterBase* Unit : Units)
		{
			if (!IsValid(Unit))
			{
				continue;
			}

			USaveableComponent* SaveComp = Unit->FindComponentByClass<USaveableComponent>();
			UUnitStatsComponent* Stats   = Unit->GetUnitStats();
			UBattleModeComponent* BM     = Unit->GetBattleModeComponent();

			if (!SaveComp || !Stats || !BM)
			{
				continue;
			}

			const FBattleUnitState* State = BattleState ? BattleState->FindUnitState(Unit) : nullptr;

			const int32 StableSourceIndex = FindStableSourceIndexForUnit(Unit, Team);
			if (StableSourceIndex == INDEX_NONE)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[CombatManager] CollectCombatSaveData: missing stable SourceIndex for %s"),
					*GetNameSafe(Unit)
				);
				continue;
			}

			FSavedCombatUnitState UnitData;
			UnitData.ActorGuid                 = SaveComp->GetOrCreateGuid();
			UnitData.ActorClass                = TSoftClassPtr<ACharacterBase>(Unit->GetClass());
			UnitData.Team                      = State ? State->Team : Team;
			UnitData.SourceIndex               = StableSourceIndex;
			UnitData.CurrentHP                 = State ? State->CurrentHP : Stats->GetCurrentHP();
			UnitData.CurrentActionPoints       = State ? State->CurrentActionPoints : 0;
			UnitData.bWasDead                  = State ? !State->IsAlive() : (Stats->GetCurrentHP() <= 0);
			UnitData.TemporaryAttributeBonuses = Stats->TemporaryAttributeBonuses;
			UnitData.Cooldowns                 = BM->CollectCooldownSaveData();

			if (Grid)
			{
				const FIntPoint GridCoord = Grid->WorldToGrid(Unit->GetActorLocation());
				if (Grid->GetTile(GridCoord))
				{
					UnitData.bHasSavedGridCoord = true;
					UnitData.SavedGridCoord = GridCoord;
				}
			}

			if (State)
			{
				for (const FActiveStatusEffect& Effect : State->ActiveStatusEffects)
				{
					if (!Effect.IsValid())
					{
						continue;
					}

					FSavedActiveStatusEffect SavedEffect;
					SavedEffect.DefinitionPath              = FSoftObjectPath(Effect.Definition->GetPathName());
					SavedEffect.CasterTeam                  = Effect.CasterTeam;
					SavedEffect.TurnsRemaining              = Effect.TurnsRemaining;
					SavedEffect.ShieldHP                    = Effect.ShieldHP;
					SavedEffect.CachedCasterStatValue       = Effect.CachedCasterStatValue;
					SavedEffect.CachedMovementRangeSnapshot = Effect.CachedMovementRangeSnapshot;
					SavedEffect.CachedStatDelta             = Effect.CachedStatDelta;
					SavedEffect.InstanceId                  = Effect.InstanceId;

					if (ACharacterBase* Caster = Effect.Caster.Get())
					{
						if (USaveableComponent* CasterSave =
							Caster->FindComponentByClass<USaveableComponent>())
						{
							SavedEffect.CasterGuid = CasterSave->GetOrCreateGuid();
						}
					}

					UnitData.ActiveStatusEffects.Add(MoveTemp(SavedEffect));
				}
			}

			OutSession.UnitStates.Add(MoveTemp(UnitData));
		}
	};

	CollectUnits(SpawnedAllies, EBattleUnitTeam::Ally);
	CollectUnits(SpawnedEnemies, EBattleUnitTeam::Enemy);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[CombatManager] CollectCombatSaveData: serialized %d units"),
		OutSession.UnitStates.Num()
	);
}

void UCombatManager::RestoreCombatFromSave(const FSavedCombatSession& SavedSession)
{
	TMap<FGuid, const FSavedCombatUnitState*> ByGuid;
	for (const FSavedCombatUnitState& Entry : SavedSession.UnitStates)
	{
		if (Entry.ActorGuid.IsValid())
		{
			ByGuid.Add(Entry.ActorGuid, &Entry);
		}
	}

	auto RestoreUnits = [&](const TArray<ACharacterBase*>& Units)
	{
		for (ACharacterBase* Unit : Units)
		{
			if (!IsValid(Unit)) { continue; }

			USaveableComponent* SaveComp = Unit->FindComponentByClass<USaveableComponent>();
			UUnitStatsComponent* Stats   = Unit->GetUnitStats();
			UBattleModeComponent* BM     = Unit->GetBattleModeComponent();

			if (!SaveComp || !Stats || !BM) { continue; }

			const FSavedCombatUnitState* Saved = ByGuid.FindRef(SaveComp->GetOrCreateGuid());
			if (!Saved) { continue; }

			Stats->TemporaryAttributeBonuses = Saved->TemporaryAttributeBonuses;
			BM->RestoreCooldowns(Saved->Cooldowns);

			UE_LOG(
				LogTemp,
				Log,
				TEXT("[CombatManager] RestoreCombatFromSave: restored %s — %d cooldown(s)"),
				*GetNameSafe(Unit),
				Saved->Cooldowns.Num()
			);
		}
	};

	RestoreUnits(SpawnedAllies);
	RestoreUnits(SpawnedEnemies);
}