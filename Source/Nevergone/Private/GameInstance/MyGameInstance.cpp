// Copyright Xyzto Works


#include "GameInstance/MyGameInstance.h"
#include "GameInstance/GameContextManager.h"

#include "EngineUtils.h"
#include "ActorComponents/SaveableComponent.h"
#include "GameInstance/MySaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"


void UMyGameInstance::Init()
{
	Super::Init();
	
	const FString SlotName = TEXT("MainSlot");

	if (!LoadSaveSlot(SlotName))
	{
		UE_LOG(LogTemp, Warning, TEXT("No save found, creating new one"));
		CreateNewSave(SlotName);
	}
	
	GameContextManager = NewObject<UGameContextManager>(this);
}

void UMyGameInstance::Shutdown()
{
	OnSaveRequested.Broadcast();
	CommitSave();
	
	Super::Shutdown();
}

void UMyGameInstance::RequestSaveGame()
{
	if (!ActiveSave)
	{
		UE_LOG(LogTemp, Warning, TEXT("RequestSaveGame failed: No active save"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Requesting manual save"));
	OnSaveRequested.Broadcast();
	
	CommitSave();

	const FString SlotName = ActiveSave->SaveSlotName;
	const bool bSuccess = UGameplayStatics::SaveGameToSlot(
		ActiveSave,
		SlotName,
		0
	);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("SaveGameToSlot failed"));
	}
}

void UMyGameInstance::RequestLoadGame()
{
	if (!ActiveSave)
	{
		UE_LOG(LogTemp, Warning, TEXT("RequestLoadGame failed: No active save"));
		return;
	}

	OnSaveLoaded.Broadcast();
}

UGameContextManager* UMyGameInstance::GetGameContextManager() const
{
	return GameContextManager;
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
	ActiveSave->SaveSlotName = SlotName;
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
	OnSaveRequested.Broadcast();
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