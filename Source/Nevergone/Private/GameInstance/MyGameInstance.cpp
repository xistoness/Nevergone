// Copyright Xyzto Works


#include "GameInstance/MyGameInstance.h"
#include "GameInstance/GameContextManager.h"

#include "GameInstance/MySaveGame.h"
#include "Level/LevelPortal.h"

#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/LoadingScreenWidget.h"
#include "World/WorldManagerSubsystem.h"


void UMyGameInstance::Init()
{
	Super::Init();
	
	// Bind after-map-load callback for OpenLevel transitions
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMyGameInstance::HandlePostLoadMapWithWorld);
	
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
	// Unbind to avoid dangling references on shutdown
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	
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
	// This loads from disk and then applies it.
	if (!LoadSaveSlot(ActiveSlotName))
	{
		UE_LOG(LogTemp, Warning, TEXT("RequestLoadGame failed: could not load slot '%s'"), *ActiveSlotName);
		return;
	}

	OnSaveLoaded.Broadcast();
}

void UMyGameInstance::ApplyActiveSaveToWorld()
{
	// No disk I/O. Just re-apply the already loaded save.
	if (!ActiveSave)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyActiveSaveToWorld failed: No active save"));
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
	GlobalFlags = ActiveSave->GlobalFlags;

	OnSaveLoaded.Broadcast();
	return true;
}

void UMyGameInstance::RequestLevelChange(FName TargetLevel, const FLevelTransitionContext& Context)
{
	if (TransitionState != ELevelTransitionState::Idle)
	{
		UE_LOG(LogTemp, Warning, TEXT("RequestLevelChange ignored: already transitioning"));
		return;
	}

	PendingLevelTransition = Context;
	PendingLevelTransition.ToLevel = TargetLevel;

	// Mark that we want to apply an entry point after the next map loads
	bHasPendingEntryPoint = !PendingLevelTransition.EntryPortalID.IsNone();

	BeginLevelTransition();

	UGameplayStatics::OpenLevel(this, TargetLevel);
}

void UMyGameInstance::BeginLevelTransition()
{
	TransitionState = ELevelTransitionState::Transitioning;

	ShowLoadingScreen();
	SetInputLocked(true);

	AutoSaveBeforeTravel();
}

void UMyGameInstance::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (!LoadedWorld)
	{
		return;
	}

	// If we are transitioning, finish the transition pipeline.
	if (TransitionState == ELevelTransitionState::Transitioning)
	{
		EndLevelTransition(LoadedWorld);
		return;
	}

	// Otherwise, this is likely the initial map load. Restore once.
	if (!bDidInitialWorldRestore)
	{
		if (UWorldManagerSubsystem* WorldMgr = LoadedWorld->GetSubsystem<UWorldManagerSubsystem>())
		{
			WorldMgr->RestoreWorldState();
		}

		ApplyPersistentStateToWorld(LoadedWorld);
		bDidInitialWorldRestore = true;
	}
}

void UMyGameInstance::EndLevelTransition(UWorld* LoadedWorld)
{
	if (!LoadedWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("EndLevelTransition failed: LoadedWorld is null"));
		TransitionState = ELevelTransitionState::Idle;
		HideLoadingScreen();
		SetInputLocked(false);
		return;
	}

	// 1) Restore world from save (actors, flags, etc.)
	if (ActiveSave)
	{
		if (UWorldManagerSubsystem* WorldMgr = LoadedWorld->GetSubsystem<UWorldManagerSubsystem>())
		{
			WorldMgr->RestoreWorldState();
		}
	}

	// 2) If this travel had an entry point tag, override the player position
	// Defer entry placement to next tick to let pawn/camera initialize properly
	FTimerHandle Tmp;
	LoadedWorld->GetTimerManager().SetTimerForNextTick([this, LoadedWorld]()
	{
		ApplyPendingEntryPoint(LoadedWorld);
	});

	// 3) Apply global state (party UI, flags, etc.)
	ApplyPersistentStateToWorld(LoadedWorld);

	HideLoadingScreen();
	SetInputLocked(false);

	TransitionState = ELevelTransitionState::Idle;
}

void UMyGameInstance::ShowLoadingScreen()
{
	if (LoadingWidgetInstance || !LoadingWidgetClass)
	{
		return;
	}

	LoadingWidgetInstance = CreateWidget< ULoadingScreenWidget>(this, LoadingWidgetClass);
	if (LoadingWidgetInstance)
	{
		LoadingWidgetInstance->AddToViewport(9999);
		LoadingWidgetInstance->OnLoadingStarted();
	}
}

void UMyGameInstance::HideLoadingScreen()
{
	if (!LoadingWidgetInstance)
	{
		return;
	}

	LoadingWidgetInstance->OnLoadingFinished();
	LoadingWidgetInstance->RemoveFromParent();
	LoadingWidgetInstance = nullptr;
}

void UMyGameInstance::SetInputLocked(bool bLocked)
{
	if (!GEngine)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return;
	}

	PC->SetIgnoreMoveInput(bLocked);
	PC->SetIgnoreLookInput(bLocked);

	// Optional: also hide mouse, depends on your UX
	// PC->bShowMouseCursor = !bLocked;
}

void UMyGameInstance::AutoSaveBeforeTravel()
{
	// If your design says "always autosave before changing levels", do it here.
	// Keep it minimal: broadcast + commit + SaveGameToSlot.
	// Do not call OpenLevel until after you commit to ActiveSave.

	OnSaveRequested.Broadcast();
	CommitSave();

	if (!ActiveSave)
	{
		return;
	}

	const bool bSuccess = UGameplayStatics::SaveGameToSlot(ActiveSave, ActiveSave->SaveSlotName, 0);
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("AutoSaveBeforeTravel: SaveGameToSlot failed"));
	}
}

void UMyGameInstance::ApplyPendingEntryPoint(UWorld* World)
{
	// If there's no pending entry point, do nothing
	if (!bHasPendingEntryPoint)
	{
		return;
	}

	// Clear immediately to avoid reapplying on future loads
	bHasPendingEntryPoint = false;

	if (!World)
	{
		return;
	}

	if (PendingLevelTransition.EntryPortalID.IsNone())
	{
		return;
	}

	// Find the portal with the requested PortalID
	ALevelPortal* EntryPortal = nullptr;

	for (TActorIterator<ALevelPortal> It(World); It; ++It)
	{
		ALevelPortal* Portal = *It;
		if (!IsValid(Portal))
		{
			continue;
		}

		if (Portal->PortalID == PendingLevelTransition.EntryPortalID)
		{
			EntryPortal = Portal;
			break;
		}
	}

	if (!EntryPortal)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyPendingEntryPoint: No LevelPortal found with PortalID '%s'"),
			*PendingLevelTransition.EntryPortalID.ToString());
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyPendingEntryPoint: No player pawn found"));
		return;
	}

	// Use the portal's entry transform (actor transform + configurable offset)
	const FTransform TargetTransform = EntryPortal->GetEntryTransform();
	Pawn->TeleportTo(TargetTransform.GetLocation(), TargetTransform.Rotator());

	PC->SetControlRotation(TargetTransform.Rotator());
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