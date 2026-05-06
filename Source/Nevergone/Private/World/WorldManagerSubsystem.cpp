// Copyright Xyzto Works


#include "World/WorldManagerSubsystem.h"
#include "ActorComponents/SaveableComponent.h"
#include "GameInstance/GameContextManager.h"
#include "GameInstance/MyGameInstance.h"
#include "GameInstance/MySaveGame.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"


void UWorldManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnWorldInitializedActors.AddUObject(
		this,
		&UWorldManagerSubsystem::HandleWorldInitializedActors
	);
}

void UWorldManagerSubsystem::HandleSaveLoaded()
{
	// During a mid-combat restore, RestoreWorldState was already called by
	// HandleWorldInitializedActors. Running it again would re-spawn combat units.
	if (bDidRestoreMidCombat) { return; }

	RestoreWorldState();
}

void UWorldManagerSubsystem::HandlePartyRestored(const FPartyData& PartyData)
{
	// TODO: party changes can affect companion spawns, etc.
}

void UWorldManagerSubsystem::CollectWorldSaveData()
{
	if (!GameInstance) return;
	UWorld* World = GetWorld();
	if (!World) return;

	const FName LevelName = GetSanitizedLevelKey();
	TArray<FActorSaveData> CollectedActors;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		USaveableComponent* SaveComp = It->FindComponentByClass<USaveableComponent>();
		if (!SaveComp) continue;

		FActorSaveData Data;
		SaveComp->WriteSaveData(Data);
		CollectedActors.Add(Data);
	}

	GameInstance->SetSavedActorsForLevel(LevelName, CollectedActors);
}

void UWorldManagerSubsystem::HandleWorldInitializedActors(const FActorsInitializedParams& Params)
{
	UWorld* LoadedWorld = Params.World;
	if (!LoadedWorld) return;

	UMyGameInstance* MyGI = LoadedWorld->GetGameInstance<UMyGameInstance>();
	if (!MyGI) return;

	GameInstance = MyGI;

	GameInstance->OnSaveLoaded.RemoveAll(this);
	GameInstance->OnSaveLoaded.AddUObject(this, &UWorldManagerSubsystem::HandleSaveLoaded);

	GameInstance->OnSaveRequested.RemoveAll(this);
	GameInstance->OnSaveRequested.AddUObject(this, &UWorldManagerSubsystem::CollectWorldSaveData);

	GameInstance->OnPartyChanged.RemoveAll(this);
	GameInstance->OnPartyChanged.AddUObject(this, &UWorldManagerSubsystem::HandlePartyRestored);

	RestoreWorldState();

	// NOTE: GameContextManager::HandleSaveLoaded is NOT called here —
	// GetAuthGameMode() returns null at this point. It is called from
	// TowerFloorGameMode::BeginPlay instead.
	bDidRestoreMidCombat = true;

	GameInstance->OnPostLevelLoad();
}

void UWorldManagerSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.RemoveAll(this);

	if (GameInstance)
	{
		GameInstance->OnSaveRequested.RemoveAll(this);
		GameInstance->OnSaveLoaded.RemoveAll(this);
		GameInstance->OnPartyChanged.RemoveAll(this);
		GameInstance = nullptr;
	}

	bDidRestoreMidCombat = false;

	ClearCachedData();

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// RestoreWorldState
// ---------------------------------------------------------------------------

void UWorldManagerSubsystem::RestoreWorldState()
{
	if (!GameInstance) return;

	const FName LevelName = GetSanitizedLevelKey();

	if (!GameInstance->HasLevelBeenVisited(LevelName))
	{
		// First visit: snapshot the original level state, register as visited,
		// and return. Nothing to restore — the level is in its default state.
		UE_LOG(LogTemp, Log,
			TEXT("[WorldManagerSubsystem] RestoreWorldState: first visit to '%s' — taking snapshot."),
			*LevelName.ToString());

		CollectWorldSaveData();
		GameInstance->MarkLevelVisited(LevelName);
		CallPostRestore();
		return;
	}

	// Subsequent visits: diff saved state against the live world.
	RestoreSaveableActors();
	ResolveActorConflicts();
	SpawnMissingActors();
	CallPostRestore();
}

// ---------------------------------------------------------------------------
// RestoreSaveableActors
// ---------------------------------------------------------------------------

void UWorldManagerSubsystem::RestoreSaveableActors()
{
	if (!GameInstance) return;
	UWorld* World = GetWorld();
	if (!World) return;

	const FName LevelName = GetSanitizedLevelKey();
	const TArray<FActorSaveData>& SavedActors = GameInstance->GetSavedActorsForLevel(LevelName);

	// Build an identifier lookup for O(1) access during iteration.
	TMap<FName, const FActorSaveData*> SavedByIdentifier;
	for (const FActorSaveData& Data : SavedActors)
	{
		if (!Data.ActorIdentifier.IsNone())
			SavedByIdentifier.Add(Data.ActorIdentifier, &Data);
	}

	// Collect actors to destroy after iteration — never mutate the world mid-loop.
	TArray<AActor*> ActorsToDestroy;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		USaveableComponent* SaveComp = Actor->FindComponentByClass<USaveableComponent>();
		if (!SaveComp) continue;

		const FName Identifier = SaveComp->GetSaveIdentifier();

		if (const FActorSaveData* Data = SavedByIdentifier.FindRef(Identifier))
		{
			// Actor exists in both world and save — restore its state.
			SaveComp->ReadSaveData(*Data);
		}
		else if (!SaveComp->bPersistAcrossLoads)
		{
			// Actor is level-placed (bPersistAcrossLoads = false) and absent from
			// the current save. Since placed actors have stable FName identifiers,
			// absence from the save means it was collected in a previous session.
			//
			// Runtime-spawned actors (pawn, dropped items) have bPersistAcrossLoads = true
			// and are never destroyed here — they are handled by SpawnMissingActors.
			UE_LOG(LogTemp, Log,
				TEXT("[WorldManagerSubsystem] RestoreSaveableActors: '%s' (id='%s') was collected — destroying."),
				*GetNameSafe(Actor), *Identifier.ToString());
			ActorsToDestroy.Add(Actor);
		}
	}

	for (AActor* Actor : ActorsToDestroy)
	{
		Actor->Destroy();
	}
}

// ---------------------------------------------------------------------------
// SpawnMissingActors
// ---------------------------------------------------------------------------

void UWorldManagerSubsystem::SpawnMissingActors()
{
	if (!GameInstance) return;
	UWorld* World = GetWorld();
	if (!World) return;

	const FName CurrentLevel = GetSanitizedLevelKey();
	const TArray<FActorSaveData>& SavedActors = GameInstance->GetSavedActorsForLevel(CurrentLevel);

	for (const FActorSaveData& Data : SavedActors)
	{
		if (Data.ActorClass.IsNull()) continue;

		// Use the bPersistAcrossLoads value recorded at save time — NOT the CDO.
		if (!Data.bPersistAcrossLoads) continue;

		// Check if the actor already exists in the world.
		bool bExists = false;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (USaveableComponent* SaveComp = It->FindComponentByClass<USaveableComponent>())
			{
				if (SaveComp->GetSaveIdentifier() == Data.ActorIdentifier)
				{
					bExists = true;
					break;
				}
			}
		}

		if (bExists) continue;

		UClass* ActorClass = Data.ActorClass.LoadSynchronous();
		if (!ActorClass) continue;

		AActor* SpawnedActor = World->SpawnActor<AActor>(ActorClass, Data.Transform);
		if (!SpawnedActor) continue;

		if (USaveableComponent* SaveComp = SpawnedActor->FindComponentByClass<USaveableComponent>())
		{
			// Reclaim the saved identifier so this new instance is recognized
			// on the next RestoreSaveableActors pass.
			SaveComp->SetSaveIdentifier(Data.ActorIdentifier);
			SaveComp->ReadSaveData(Data);
		}

		UE_LOG(LogTemp, Log,
			TEXT("[WorldManagerSubsystem] SpawnMissingActors: respawned '%s' (id='%s') at %s."),
			*GetNameSafe(SpawnedActor),
			*Data.ActorIdentifier.ToString(),
			*Data.Transform.GetTranslation().ToString());
	}
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

void UWorldManagerSubsystem::ResolveActorConflicts()
{
	// TODO: handle duplicate identifiers and other edge cases.
}

void UWorldManagerSubsystem::CallPostRestore()
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor)) continue;

		USaveableComponent* SaveComp = Actor->FindComponentByClass<USaveableComponent>();
		if (!SaveComp) continue;

		SaveComp->OnPostRestore();
	}
}

void UWorldManagerSubsystem::ClearCachedData()
{
	// TODO
}

FName UWorldManagerSubsystem::GetSanitizedLevelKey() const
{
	UWorld* World = GetWorld();
	if (!World) { return NAME_None; }

	FString PackageName = World->GetOutermost()->GetName();

	// Strip PIE prefixes so the key is stable across editor sessions.
	PackageName.ReplaceInline(TEXT("UEDPIE_0_"), TEXT(""));
	PackageName.ReplaceInline(TEXT("UEDPIE_1_"), TEXT(""));
	PackageName.ReplaceInline(TEXT("UEDPIE_2_"), TEXT(""));

	return FName(*PackageName);
}
