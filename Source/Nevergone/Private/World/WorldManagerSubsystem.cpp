// Copyright Xyzto Works


#include "World/WorldManagerSubsystem.h"
#include "GameInstance/MyGameInstance.h"
#include "GameInstance/MySaveGame.h"
#include "GameInstance/Saveable.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

void UWorldManagerSubsystem::HandleSaveLoaded()
{
	RestoreWorldState();
}

void UWorldManagerSubsystem::HandlePartyRestored(const FPartyData& PartyData)
{
	// TODO	
	// Party can affect companion spawns, etc
}

void UWorldManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Listen for actors initialized event
	FWorldDelegates::OnWorldInitializedActors.AddUObject(
		this,
		&UWorldManagerSubsystem::HandleWorldInitializedActors
	);
}

void UWorldManagerSubsystem::HandleWorldInitializedActors(
	const FActorsInitializedParams& Params
)
{
	UWorld* LoadedWorld = Params.World;
	if (!LoadedWorld)
		return;

	GameInstance = LoadedWorld->GetGameInstance<UMyGameInstance>();
	if (!GameInstance)
		return;

	// Register once per world
	GameInstance->OnSaveLoaded.AddUObject(
		this,
		&UWorldManagerSubsystem::HandleSaveLoaded
	);

	GameInstance->OnPartyChanged.AddUObject(
		this,
		&UWorldManagerSubsystem::HandlePartyRestored
	);

	GameInstance->OnPostLevelLoad();

	RestoreWorldState();
}

void UWorldManagerSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.RemoveAll(this);

	ClearCachedData();

	Super::Deinitialize();
}

void UWorldManagerSubsystem::RestoreWorldState()
{
	RegisterSaveableActors();
	RestoreSaveableActors();
	ResolveActorConflicts();
	SpawnMissingActors();
}

void UWorldManagerSubsystem::RegisterSaveableActors() const
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->Implements<USaveable>())
		{
			// Index by GUID, Tag or ID
		}
	}
}

void UWorldManagerSubsystem::ClearCachedData()
{
	// TODO 
	//SaveableActors.Empty();
	//ActorsRestoredThisLoad.Empty();
	//PendingSpawnActors.Empty();
}

void UWorldManagerSubsystem::RestoreSaveableActors()
{
	if (!GameInstance)
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	const TArray<FActorSaveData>& SavedActors = GameInstance->GetSavedActors();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;

		if (!Actor->Implements<USaveable>())
			continue;

		ISaveable* Saveable = Cast<ISaveable>(Actor);
		if (!Saveable)
			continue;

		const FGuid ActorGuid = Saveable->GetOrCreateGuid();

		for (const FActorSaveData& Data : SavedActors)
		{
			if (Data.ActorGuid == ActorGuid)
			{
				Saveable->ReadSaveData(Data);
				break;
			}
		}
	}
}

void UWorldManagerSubsystem::ResolveActorConflicts()
{
	// TODO	
	// Duplicates
	// Exists in saved game but not in the world
	// Exists in the world but not in the saved game
}

void UWorldManagerSubsystem::SpawnMissingActors()
{
	if (!GameInstance)
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	const TArray<FActorSaveData>& SavedActors = GameInstance->GetSavedActors();
	const FName CurrentLevel = World->GetFName();

	for (const FActorSaveData& Data : SavedActors)
	{
		if (Data.LevelName != CurrentLevel)
			continue;

		bool bAlreadyExists = false;

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->Implements<USaveable>())
				continue;

			ISaveable* Saveable = Cast<ISaveable>(*It);
			if (Saveable && Saveable->GetOrCreateGuid() == Data.ActorGuid)
			{
				bAlreadyExists = true;
				break;
			}
		}

		if (bAlreadyExists)
			continue;

		if (!Data.ActorClass.IsNull())
		{
			AActor* SpawnedActor =
				World->SpawnActor<AActor>(
					Data.ActorClass.LoadSynchronous(),
					Data.Transform
				);

			if (SpawnedActor && SpawnedActor->Implements<USaveable>())
			{
				ISaveable* Saveable = Cast<ISaveable>(SpawnedActor);
				Saveable->SetActorGuid(Data.ActorGuid);
				Saveable->ReadSaveData(Data);
			}
		}
	}
}