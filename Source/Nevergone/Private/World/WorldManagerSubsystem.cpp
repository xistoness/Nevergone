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

	// Listen for actors initialized event
	FWorldDelegates::OnWorldInitializedActors.AddUObject(
		this,
		&UWorldManagerSubsystem::HandleWorldInitializedActors
	);
}

void UWorldManagerSubsystem::HandleSaveLoaded()
{
    // During a mid-combat restore, RestoreWorldState was already called by
    // HandleWorldInitializedActors. Running it again via OnSaveLoaded would
    // trigger SpawnMissingActors and re-spawn combat units that were just
    // destroyed/replaced by the restore. Skip it entirely.
    if (bDidRestoreMidCombat) { return; }

	RestoreWorldState();
}

void UWorldManagerSubsystem::HandlePartyRestored(const FPartyData& PartyData)
{
	// TODO	
	// Party can affect companion spawns, etc
}

void UWorldManagerSubsystem::CollectWorldSaveData()
{
	if (!GameInstance) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const FName LevelName = World->GetOutermost()->GetFName();
	TArray<FActorSaveData> CollectedActors;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (USaveableComponent* SaveComp = It->FindComponentByClass<USaveableComponent>())
		{
			if (!SaveComp->bPersistAcrossLoads) continue;
			FActorSaveData Data;
			SaveComp->WriteSaveData(Data);
			CollectedActors.Add(Data);
		}
	}

	// Only overwrites data for the current level — other levels are untouched
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

	// Restore world state here — GameInstance is guaranteed valid at this point.
	// This covers both the initial load and every level transition return.
	RestoreWorldState();

    // After world actors are restored, give GameContextManager a chance to
    // detect a mid-combat save and reconstruct the battle session.
    // Guard with bDidRestoreMidCombat so streaming sublevel loads don't
    // trigger a second restore on the same world.
    if (!bDidRestoreMidCombat)
    {
        bDidRestoreMidCombat = true;

        if (UGameInstance* GI = LoadedWorld->GetGameInstance())
        {
            if (UGameContextManager* ContextMgr = GI->GetSubsystem<UGameContextManager>())
            {
                ContextMgr->HandleSaveLoaded();
            }
        }
    }

	GameInstance->OnPostLevelLoad();
}

void UWorldManagerSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.RemoveAll(this);

	// Remove GameInstance bindings before the world is torn down.
	// AutoSaveBeforeTravel broadcasts OnSaveRequested while transitioning,
	// which happens after this subsystem is deinitialized. Without this
	// cleanup the delegate fires into a destroyed object.
	if (GameInstance)
	{
		GameInstance->OnSaveRequested.RemoveAll(this);
		GameInstance->OnSaveLoaded.RemoveAll(this);
		GameInstance->OnPartyChanged.RemoveAll(this);
		GameInstance = nullptr;
	}

    // Reset so the next world load can trigger a mid-combat restore if needed.
    bDidRestoreMidCombat = false;

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
	if (!GameInstance) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const FName LevelName = World->GetOutermost()->GetFName();
	const TArray<FActorSaveData>& SavedActors = GameInstance->GetSavedActorsForLevel(LevelName);

	TMap<FGuid, const FActorSaveData*> SavedByGuid;
	for (const FActorSaveData& Data : SavedActors)
	{
		if (Data.ActorGuid.IsValid())
			SavedByGuid.Add(Data.ActorGuid, &Data);
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		USaveableComponent* SaveComp = Actor->FindComponentByClass<USaveableComponent>();
		if (!SaveComp) continue;

		const FGuid Guid = SaveComp->GetOrCreateGuid();
		if (const FActorSaveData* Data = SavedByGuid.FindRef(Guid))
			SaveComp->ReadSaveData(*Data);
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
	if (!GameInstance) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const FName CurrentLevel = World->GetOutermost()->GetFName();
	const TArray<FActorSaveData>& SavedActors = GameInstance->GetSavedActorsForLevel(CurrentLevel);

	for (const FActorSaveData& Data : SavedActors)
	{
		bool bExists = false;

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (USaveableComponent* SaveComp = It->FindComponentByClass<USaveableComponent>())
			{
				if (SaveComp->GetOrCreateGuid() == Data.ActorGuid)
				{
					bExists = true;
					break;
				}
			}
		}

		if (bExists || Data.ActorClass.IsNull()) continue;

		AActor* SpawnedActor = World->SpawnActor<AActor>(
			Data.ActorClass.LoadSynchronous(),
			Data.Transform
		);

		if (!SpawnedActor) continue;

		if (USaveableComponent* SaveComp = SpawnedActor->FindComponentByClass<USaveableComponent>())
		{
			SaveComp->SetActorGuid(Data.ActorGuid);
			SaveComp->ReadSaveData(Data);
		}
	}
}