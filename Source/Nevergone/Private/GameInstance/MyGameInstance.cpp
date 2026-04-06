// Copyright Xyzto Works


#include "GameInstance/MyGameInstance.h"
#include "GameInstance/GameContextManager.h"
#include "GameInstance/MySaveGame.h"
#include "Data/SaveSlotInfo.h"
#include "Level/LevelPortal.h"

#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Party/PartyManagerSubsystem.h"
#include "Widgets/LoadingScreenWidget.h"
#include "World/WorldManagerSubsystem.h"


void UMyGameInstance::Init()
{
	Super::Init();

	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMyGameInstance::HandlePostLoadMapWithWorld);

	// On first boot, find the most recent slot and load it.
	// If no slot exists yet, create a fresh one in slot 0.
	bool bLoaded = false;
	FDateTime MostRecentTime = FDateTime::MinValue();
	FString MostRecentSlotName;

	for (int32 i = 0; i < MaxSaveSlots; ++i)
	{
		const FString SlotName = GetSlotNameForIndex(i);
		if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
			continue;

		UMySaveGame* SlotSave = Cast<UMySaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
		if (SlotSave && SlotSave->LastSavedAt > MostRecentTime)
		{
			MostRecentTime     = SlotSave->LastSavedAt;
			MostRecentSlotName = SlotName;
			bLoaded            = true;
		}
	}

	if (bLoaded)
	{
		UE_LOG(LogTemp, Log, TEXT("[MyGameInstance] Init: loading most recent slot '%s'"), *MostRecentSlotName);
		LoadSaveSlot(MostRecentSlotName);
	}
	else
	{
		const FString DefaultSlot = GetSlotNameForIndex(0);
		UE_LOG(LogTemp, Warning, TEXT("[MyGameInstance] Init: no save found, creating new one in '%s'"), *DefaultSlot);
		CreateNewSave(DefaultSlot);
	}

	GameContextManager = NewObject<UGameContextManager>(this);

    // Note: GameContextManager::HandleSaveLoaded is called directly by
    // WorldManagerSubsystem::HandleWorldInitializedActors after RestoreWorldState(),
    // NOT via OnSaveLoaded, because the world must be fully ready first.
	
	// Pass audio configuration to the AudioSubsystem — it cannot hold
	// EditDefaultsOnly properties directly since it has no Blueprint asset.
	if (UAudioSubsystem* Audio = GetSubsystem<UAudioSubsystem>())
	{
		Audio->DefaultHitSFX   = DefaultHitSFX;
		Audio->DefaultDeathSFX = DefaultDeathSFX;
		Audio->DefaultHealSFX  = DefaultHealSFX;
		Audio->MasterSoundMix  = MasterSoundMix;
		Audio->UISoundMap      = UISoundMap;

		UE_LOG(LogTemp, Log, TEXT("[MyGameInstance] Audio configuration passed to AudioSubsystem."));
	}
}

void UMyGameInstance::Shutdown()
{
	// Unbind to avoid dangling references on shutdown
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	
	OnSaveRequested.Broadcast();
	CommitSave();
	
	Super::Shutdown();
}

bool UMyGameInstance::HasSaveGame() const
{
	for (int32 i = 0; i < MaxSaveSlots; ++i)
	{
		if (UGameplayStatics::DoesSaveGameExist(GetSlotNameForIndex(i), 0))
		{
			return true;
		}
	}
	return false;
}
 
void UMyGameInstance::RequestNewGame()
{
	int32 TargetSlot = FindNextAvailableSlot();

	ActiveSlotName = GetSlotNameForIndex(TargetSlot);
	CreateNewSave(ActiveSlotName);

	UE_LOG(LogTemp, Log, TEXT("[MyGameInstance] RequestNewGame: new save in slot %d ('%s')"),
		TargetSlot, *ActiveSlotName);
}

int32 UMyGameInstance::FindNextAvailableSlot() const
{
	// First pass: find an empty slot
	for (int32 i = 0; i < MaxSaveSlots; ++i)
	{
		if (!UGameplayStatics::DoesSaveGameExist(GetSlotNameForIndex(i), 0))
		{
			return i;
		}
	}

	// No empty slot — find the oldest save to overwrite
	int32 OldestSlot = 0;
	FDateTime OldestTime = FDateTime::MaxValue();

	for (int32 i = 0; i < MaxSaveSlots; ++i)
	{
		UMySaveGame* SlotSave = Cast<UMySaveGame>(
			UGameplayStatics::LoadGameFromSlot(GetSlotNameForIndex(i), 0)
		);

		if (SlotSave && SlotSave->LastSavedAt < OldestTime)
		{
			OldestTime = SlotSave->LastSavedAt;
			OldestSlot = i;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[MyGameInstance] FindNextAvailableSlot: no empty slot, overwriting oldest (slot %d)"),
		OldestSlot);

	return OldestSlot;
}

void UMyGameInstance::RequestContinue()
{
	// Find the most recently saved slot
	int32 MostRecentSlot = -1;
	FDateTime MostRecentTime = FDateTime::MinValue();

	for (int32 i = 0; i < MaxSaveSlots; ++i)
	{
		const FString SlotName = GetSlotNameForIndex(i);
		if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
		{
			continue;
		}

		UMySaveGame* SlotSave = Cast<UMySaveGame>(
			UGameplayStatics::LoadGameFromSlot(SlotName, 0)
		);

		if (SlotSave && SlotSave->LastSavedAt > MostRecentTime)
		{
			MostRecentTime = SlotSave->LastSavedAt;
			MostRecentSlot = i;
		}
	}

	if (MostRecentSlot == -1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MyGameInstance] RequestContinue: no valid save found"));
		return;
	}

	LoadSlotByIndex(MostRecentSlot);

	UE_LOG(LogTemp, Log, TEXT("[MyGameInstance] RequestContinue: loaded most recent save (slot %d)"),
		MostRecentSlot);
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

FSaveSlotInfo UMyGameInstance::GetMostRecentSaveInfo() const
{
	FSaveSlotInfo BestInfo;
    FDateTime MostRecentTime = FDateTime::MinValue();

    for (int32 i = 0; i < MaxSaveSlots; ++i)
    {
        const FString SlotName = GetSlotNameForIndex(i);
        if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) continue;

        UMySaveGame* SlotSave = Cast<UMySaveGame>(
            UGameplayStatics::LoadGameFromSlot(SlotName, 0)
        );

        if (SlotSave && SlotSave->LastSavedAt > MostRecentTime)
        {
            MostRecentTime          = SlotSave->LastSavedAt;
            BestInfo.bIsOccupied    = true;
            BestInfo.SlotIndex      = i;
            BestInfo.SlotName       = SlotName;
            BestInfo.DisplayName    = SlotSave->SaveDisplayName.IsEmpty()
                ? FString::Printf(TEXT("Save %d"), i + 1)
                : SlotSave->SaveDisplayName;

            const int32 TotalMinutes = FMath::FloorToInt(SlotSave->PlaytimeSeconds / 60.f);
            BestInfo.PlaytimeFormatted = FString::Printf(TEXT("%dh %dm"),
                TotalMinutes / 60, TotalMinutes % 60);

            const FTimespan Delta   = FDateTime::UtcNow() - SlotSave->LastSavedAt;
            const int32 MinutesAgo  = FMath::FloorToInt(Delta.GetTotalMinutes());
            const int32 HoursAgo    = FMath::FloorToInt(Delta.GetTotalHours());
            const int32 DaysAgo     = FMath::FloorToInt(Delta.GetTotalDays());

            if (MinutesAgo < 1)
                BestInfo.LastSavedFormatted = TEXT("Just now");
            else if (MinutesAgo < 60)
                BestInfo.LastSavedFormatted = FString::Printf(TEXT("%d min ago"), MinutesAgo);
            else if (HoursAgo < 24)
                BestInfo.LastSavedFormatted = FString::Printf(TEXT("%dh ago"), HoursAgo);
            else
                BestInfo.LastSavedFormatted = FString::Printf(TEXT("%d day%s ago"),
                    DaysAgo, DaysAgo == 1 ? TEXT("") : TEXT("s"));

            BestInfo.SavedLevelName = SlotSave->SavedLevelName;
        }
    }

    return BestInfo;
}

FString UMyGameInstance::GetSlotNameForIndex(int32 SlotIndex) const
{
    return FString::Printf(TEXT("Slot_%d"), SlotIndex);
}
 
TArray<FSaveSlotInfo> UMyGameInstance::GetAllSaveSlots() const
{
    TArray<FSaveSlotInfo> Result;
 
    for (int32 i = 0; i < MaxSaveSlots; ++i)
    {
        FSaveSlotInfo Info;
        Info.SlotIndex = i;
        Info.SlotName  = GetSlotNameForIndex(i);
 
        const bool bExists = UGameplayStatics::DoesSaveGameExist(Info.SlotName, 0);
        Info.bIsOccupied   = bExists;
 
        if (bExists)
        {
            // Load only to read metadata — do not apply to the world
            UMySaveGame* SlotSave = Cast<UMySaveGame>(
                UGameplayStatics::LoadGameFromSlot(Info.SlotName, 0)
            );
 
            if (SlotSave)
            {
                Info.DisplayName    = SlotSave->SaveDisplayName.IsEmpty()
                    ? FString::Printf(TEXT("Save %d"), i + 1)
                    : SlotSave->SaveDisplayName;
 
                Info.SavedLevelName = SlotSave->SavedLevelName;
 
                // Format playtime as "Xh Ym"
                const int32 TotalMinutes = FMath::FloorToInt(SlotSave->PlaytimeSeconds / 60.f);
                const int32 Hours        = TotalMinutes / 60;
                const int32 Minutes      = TotalMinutes % 60;
                Info.PlaytimeFormatted   = FString::Printf(TEXT("%dh %dm"), Hours, Minutes);
 
                // Format last saved as relative time
                const FTimespan Delta    = FDateTime::UtcNow() - SlotSave->LastSavedAt;
                const int32 DaysAgo      = FMath::FloorToInt(Delta.GetTotalDays());
                const int32 HoursAgo     = FMath::FloorToInt(Delta.GetTotalHours());
                const int32 MinutesAgo   = FMath::FloorToInt(Delta.GetTotalMinutes());
 
                if (MinutesAgo < 1)
                    Info.LastSavedFormatted = TEXT("Just now");
                else if (MinutesAgo < 60)
                    Info.LastSavedFormatted = FString::Printf(TEXT("%d min ago"), MinutesAgo);
                else if (HoursAgo < 24)
                    Info.LastSavedFormatted = FString::Printf(TEXT("%d hour%s ago"), HoursAgo, HoursAgo == 1 ? TEXT("") : TEXT("s"));
                else
                    Info.LastSavedFormatted = FString::Printf(TEXT("%d day%s ago"), DaysAgo, DaysAgo == 1 ? TEXT("") : TEXT("s"));
            }
        }
 
        Result.Add(Info);
    }
 
    return Result;
}
 
bool UMyGameInstance::LoadSlotByIndex(int32 SlotIndex)
{
    const FString SlotName = GetSlotNameForIndex(SlotIndex);
 
    if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
    {
        UE_LOG(LogTemp, Warning, TEXT("[MyGameInstance] LoadSlotByIndex: slot %d is empty"), SlotIndex);
        return false;
    }
 
    if (!LoadSaveSlot(SlotName))
    {
        UE_LOG(LogTemp, Error, TEXT("[MyGameInstance] LoadSlotByIndex: failed to load slot %d"), SlotIndex);
        return false;
    }
 
    UE_LOG(LogTemp, Log, TEXT("[MyGameInstance] LoadSlotByIndex: loaded slot %d ('%s')"), SlotIndex, *SlotName);
    return true;
}
 
void UMyGameInstance::DeleteSlotByIndex(int32 SlotIndex)
{
    const FString SlotName = GetSlotNameForIndex(SlotIndex);
    UGameplayStatics::DeleteGameInSlot(SlotName, 0);
    UE_LOG(LogTemp, Log, TEXT("[MyGameInstance] DeleteSlotByIndex: deleted slot %d ('%s')"), SlotIndex, *SlotName);
}
 
void UMyGameInstance::UpdateSaveMetadata(const FString& DisplayName, FName CurrentLevelName)
{
    if (!ActiveSave) return;
 
    ActiveSave->SaveDisplayName = DisplayName;
    ActiveSave->SavedLevelName  = CurrentLevelName;
    ActiveSave->LastSavedAt     = FDateTime::UtcNow();
}

FName UMyGameInstance::GetActiveSavedLevelName() const
{
	if (!ActiveSave) return NAME_None;
	return ActiveSave->SavedLevelName;
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

	ActiveSave      = NewSave;
	ActiveSlotName  = SlotName;

	ActiveSave->SaveSlotName    = SlotName;
	ActiveSave->LastSavedAt     = FDateTime::UtcNow();
	ActiveSave->PlaytimeSeconds = 0.f;
	ActiveSave->SaveDisplayName = TEXT("New Game");

	PartyData       = FPartyData();
	ProgressionData = FProgressionData();
	GlobalFlags.Empty();

	// Populate default party for a new game.
	// This runs only once — subsequent loads read from the save slot.
	// TODO: replace with proper new game initialization when that system exists.
	if (UPartyManagerSubsystem* PartyMgr = GetSubsystem<UPartyManagerSubsystem>())
	{
		PartyMgr->SyncFromGameInstance(); // clears subsystem to match empty PartyData
	}

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

	ActiveSlotName  = SlotName;
	PartyData       = ActiveSave->PartyData;
	ProgressionData = ActiveSave->ProgressionData;
	GlobalFlags     = ActiveSave->GlobalFlags;

	// Keep PartyManagerSubsystem in sync so BuildBattleParty sees the loaded data.
	if (UPartyManagerSubsystem* PartyMgr = GetSubsystem<UPartyManagerSubsystem>())
	{
		PartyMgr->SyncFromGameInstance();
	}

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

	// GetWorld() here is called from BeginLevelTransition, which is triggered
	// by RequestLevelChange -> called from a portal actor, so the world is still
	// valid and correct at this point. We capture it before OpenLevel tears it down.
	AutoSaveBeforeTravel(GetWorld());
}

void UMyGameInstance::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (!LoadedWorld) return;

	if (TransitionState == ELevelTransitionState::Transitioning)
	{
		EndLevelTransition(LoadedWorld);
		return;
	}

	// Initial load: ApplyPersistentStateToWorld only.
	// RestoreWorldState is handled by WorldManagerSubsystem::HandleWorldInitializedActors.
	if (!bDidInitialWorldRestore)
	{
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

	// RestoreWorldState is now handled by WorldManagerSubsystem::HandleWorldInitializedActors,
	// which is guaranteed to run after GameInstance is set on the subsystem.

	FTimerHandle Tmp;
	LoadedWorld->GetTimerManager().SetTimerForNextTick([this, LoadedWorld]()
	{
		ApplyPendingEntryPoint(LoadedWorld);
	});

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

	// Optional: also hide mouse, depends on UX
	// PC->bShowMouseCursor = !bLocked;
}

void UMyGameInstance::AutoSaveBeforeTravel(UWorld* WorldToSave)
{
	if (!ActiveSave)
	{
		UE_LOG(LogTemp, Error, TEXT("[MyGameInstance] AutoSaveBeforeTravel: no ActiveSave — skipping"));
		return;
	}

	if (!WorldToSave)
	{
		UE_LOG(LogTemp, Error, TEXT("[MyGameInstance] AutoSaveBeforeTravel: WorldToSave is null — skipping"));
		return;
	}

	ActiveSave->LastSavedAt = FDateTime::UtcNow();

	UE_LOG(LogTemp, Warning, TEXT("[AutoSave] Collecting from world: %s"), *WorldToSave->GetName());

	if (UWorldManagerSubsystem* WorldMgr = WorldToSave->GetSubsystem<UWorldManagerSubsystem>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AutoSave] Got subsystem, calling CollectWorldSaveData"));
		WorldMgr->CollectWorldSaveData();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[AutoSave] No WorldManagerSubsystem found in world '%s'!"), *WorldToSave->GetName());
	}

	CommitSave();

	const bool bSuccess = UGameplayStatics::SaveGameToSlot(
		ActiveSave,
		ActiveSave->SaveSlotName,
		0
	);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("[MyGameInstance] AutoSaveBeforeTravel: saved to slot '%s' from world '%s'"),
			*ActiveSave->SaveSlotName, *WorldToSave->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[MyGameInstance] AutoSaveBeforeTravel: SaveGameToSlot failed"));
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

void UMyGameInstance::SetSavedActorsForLevel(FName LevelName, const TArray<FActorSaveData>& Actors) const
{
	if (!ActiveSave) return;
	ActiveSave->SavedActorsByLevel.FindOrAdd(LevelName).Actors = Actors;
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

const TArray<FActorSaveData>& UMyGameInstance::GetSavedActorsForLevel(FName LevelName) const
{
	static TArray<FActorSaveData> Empty;
	if (!ActiveSave) return Empty;
	const FLevelSaveData* Found = ActiveSave->SavedActorsByLevel.Find(LevelName);
	return Found ? Found->Actors : Empty;
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