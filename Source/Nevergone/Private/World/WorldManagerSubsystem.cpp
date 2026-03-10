// Copyright Xyzto Works


#include "World/WorldManagerSubsystem.h"
#include "ActorComponents/SaveableComponent.h"
#include "GameInstance/MyGameInstance.h"
#include "GameInstance/MySaveGame.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"


void UWorldManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Listen for actors initialized event
	FWorldDelegates::OnWorldInitializedActors.AddUObject(
		this,
		&UWorldManagerSubsystem::HandleWorldInitializedActors
	);
}

void UWorldManagerSubsystem::HandleSaveLoaded()
{
	RestoreWorldState();
}

void UWorldManagerSubsystem::HandlePartyRestored(const FPartyData& PartyData)
{
	// TODO	
	// Party can affect companion spawns, etc
}

void UWorldManagerSubsystem::CollectWorldSaveData()
{
	if (!GameInstance)
		return;

	TArray<FActorSaveData> CollectedActors;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (USaveableComponent* SaveComp =
			It->FindComponentByClass<USaveableComponent>())
		{
			FActorSaveData Data;
			SaveComp->WriteSaveData(Data);
			CollectedActors.Add(Data);
		}
	}

	GameInstance->SetSavedActors(CollectedActors);
}

void UWorldManagerSubsystem::HandleWorldInitializedActors(
	const FActorsInitializedParams& Params
)
{
	UWorld* LoadedWorld = Params.World;
	if (!LoadedWorld)
		return;

	UMyGameInstance* MyGI =
		LoadedWorld->GetGameInstance<UMyGameInstance>();

	if (!MyGI)
		return;

	GameInstance = MyGI;

	// Bind save/load events (only once per world)
	GameInstance->OnSaveLoaded.RemoveAll(this);
	GameInstance->OnSaveLoaded.AddUObject(this,	&UWorldManagerSubsystem::HandleSaveLoaded);
	
	GameInstance->OnSaveRequested.RemoveAll(this);
	GameInstance->OnSaveRequested.AddUObject(this, &UWorldManagerSubsystem::CollectWorldSaveData);

	GameInstance->OnPartyChanged.RemoveAll(this);
	GameInstance->OnPartyChanged.AddUObject(this, &UWorldManagerSubsystem::HandlePartyRestored);

	// Notify world is ready
	GameInstance->OnPostLevelLoad();
}

void UWorldManagerSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.RemoveAll(this);

	ClearCachedData();

	Super::Deinitialize();
}

void UWorldManagerSubsystem::RestoreWorldState()
{
	RestoreSaveableActors();
	ResolveActorConflicts();
	SpawnMissingActors();
	CallPostRestore();
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

	// Index saved data by GUID
	TMap<FGuid, const FActorSaveData*> SavedByGuid;
	for (const FActorSaveData& Data : SavedActors)
	{
		if (Data.ActorGuid.IsValid())
		{
			SavedByGuid.Add(Data.ActorGuid, &Data);
		}
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;

		USaveableComponent* SaveComp =
			Actor->FindComponentByClass<USaveableComponent>();

		if (!SaveComp)
			continue;

		const FGuid Guid = SaveComp->GetOrCreateGuid();

		if (const FActorSaveData* Data = SavedByGuid.FindRef(Guid))
		{
			SaveComp->ReadSaveData(*Data);
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

void UWorldManagerSubsystem::CallPostRestore()
{
	UWorld* World = GetWorld();
	if (!World)
		return;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
			continue;

		USaveableComponent* SaveComp =
			Actor->FindComponentByClass<USaveableComponent>();

		if (!SaveComp)
			continue;
		
		SaveComp->OnPostRestore();
	}
}

void UWorldManagerSubsystem::SpawnMissingActors()
{
	if (!GameInstance)
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	const TArray<FActorSaveData>& SavedActors = GameInstance->GetSavedActors();
	const FName CurrentLevel = World->GetOutermost()->GetFName();

	for (const FActorSaveData& Data : SavedActors)
	{
		if (Data.LevelName != CurrentLevel)
			continue;

		bool bExists = false;

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (USaveableComponent* SaveComp =
				It->FindComponentByClass<USaveableComponent>())
			{
				if (SaveComp->GetOrCreateGuid() == Data.ActorGuid)
				{
					bExists = true;
					break;
				}
			}
		}

		if (bExists || Data.ActorClass.IsNull())
			continue;

		AActor* SpawnedActor = World->SpawnActor<AActor>(
			Data.ActorClass.LoadSynchronous(),
			Data.Transform
		);

		if (!SpawnedActor)
			continue;

		if (USaveableComponent* SaveComp =
			SpawnedActor->FindComponentByClass<USaveableComponent>())
		{
			SaveComp->SetActorGuid(Data.ActorGuid);
			SaveComp->ReadSaveData(Data);
		}
	}
}
