// Copyright Xyzto Works


#include "GameInstance/MyGameInstance.h"

#include "EngineUtils.h"
#include "GameInstance/MySaveGame.h"
#include "GameInstance/Saveable.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"


void UMyGameInstance::Init()
{
	Super::Init();
}

void UMyGameInstance::Shutdown()
{
	Super::Shutdown();
}

void UMyGameInstance::ClearActiveSave()
{
	ActiveSave = nullptr;
	PartyData = FPartyData();
	ProgressionData = FProgressionData();
	GlobalFlags.Empty();
}

bool UMyGameInstance::CreateNewSave(const FString& SlotName)
{
	UMySaveGame* NewSave =
		Cast<UMySaveGame>(UGameplayStatics::CreateSaveGameObject(UMySaveGame::StaticClass()));

	if (!NewSave)
		return false;

	ActiveSave = NewSave;

	// Initial state
	PartyData = FPartyData();
	ProgressionData = FProgressionData();
	GlobalFlags.Empty();

	CommitSave();

	return UGameplayStatics::SaveGameToSlot(ActiveSave, SlotName, 0);
}

void UMyGameInstance::CommitSave() const
{
	if (!ActiveSave)
		return;

	ActiveSave->PartyData = PartyData;
	ActiveSave->ProgressionData = ProgressionData;
	ActiveSave->GlobalFlags = GlobalFlags;
	ActiveSave->SavedActors.Empty();
	
	UWorld* World = GetWorld();
	if (!World)
		return;
	
	const FName CurrentLevel = FName(*World->GetMapName());

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		
		if (Actor->Implements<USaveable>())
		{
			FActorSaveData SaveData;
			ISaveable::Execute_WriteSaveData(Actor, SaveData);
			ActiveSave->SavedActors.Add(SaveData);
		}
	}
}

bool UMyGameInstance::LoadSaveSlot(const FString& SlotName)
{
	ActiveSave = Cast<UMySaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, 0)
	);

	if (!ActiveSave)
		return false;

	PartyData = ActiveSave->PartyData;
	ProgressionData = ActiveSave->ProgressionData;

	OnSaveLoaded.Broadcast();
	return true;
}

void UMyGameInstance::RequestLevelChange(
	FName TargetLevel,
	const FLevelTransitionContext& Context
)
{
	PendingLevelTransition = Context;
	PendingLevelTransition.ToLevel = TargetLevel;

	OnPreLevelUnload();
}

void UMyGameInstance::OnPreLevelUnload() const
{
	CommitSave();
}

void UMyGameInstance::OnPostLevelLoad() const
{
	ApplyPersistentStateToWorld(GetWorld());
}

void UMyGameInstance::ApplyPersistentStateToWorld(UWorld* World) const
{
	OnPartyChanged.Broadcast(PartyData);
}

void UMyGameInstance::AddOrUpdateSavedActor(const FActorSaveData& ActorData) const
{
	if (!ActiveSave || !ActorData.ActorGuid.IsValid())
		return;

	for (FActorSaveData& Existing : ActiveSave->SavedActors)
	{
		if (Existing.ActorGuid == ActorData.ActorGuid)
		{
			Existing = ActorData;
			return;
		}
	}

	ActiveSave->SavedActors.Add(ActorData);
}

// Setter Functions

void UMyGameInstance::SetActiveSave(UMySaveGame* Save)
{
	ActiveSave = Save;
}

void UMyGameInstance::SetPartyData(const FPartyData& NewData)
{
	PartyData = NewData;
	OnPartyChanged.Broadcast(PartyData);
}

void UMyGameInstance::SetProgressionData(const FProgressionData& NewData)
{
	ProgressionData = NewData;
}

void UMyGameInstance::SetGlobalFlag(FName Flag, bool bValue)
{
	GlobalFlags.Add(Flag, bValue);
}

void UMyGameInstance::SetSavedActors(const TArray<FActorSaveData>& NewActors) const
{
	if (!ActiveSave)
		return;

	ActiveSave->SavedActors = NewActors;
}

// Getter Functions

UMySaveGame* UMyGameInstance::GetActiveSave() const
{
	return ActiveSave;
}

const FPartyData& UMyGameInstance::GetPartyData() const
{
	return PartyData;
}

const FProgressionData& UMyGameInstance::GetProgressionData() const
{
	return ProgressionData;
}

bool UMyGameInstance::GetGlobalFlag(FName Flag) const
{
	if (const bool* Value = GlobalFlags.Find(Flag))
	{
		return *Value;
	}

	return false;
}

const TArray<FActorSaveData>& UMyGameInstance::GetSavedActors() const
{
	static TArray<FActorSaveData> Empty;
	return ActiveSave ? ActiveSave->SavedActors : Empty;
}

// Debug Helpers

void UMyGameInstance::Debug_PrintGlobalState() const
{
	UE_LOG(LogTemp, Warning, TEXT("=== GLOBAL STATE ==="));

	UE_LOG(LogTemp, Warning, TEXT("Party Members: %d"),
		PartyData.Members.Num());

	UE_LOG(LogTemp, Warning, TEXT("Main Quest Stage: %d"),
		ProgressionData.MainQuestStage);

	UE_LOG(LogTemp, Warning, TEXT("Global Flags: %d"),
		GlobalFlags.Num());
}

void UMyGameInstance::Debug_ResetProgression()
{
	ProgressionData = FProgressionData();
}