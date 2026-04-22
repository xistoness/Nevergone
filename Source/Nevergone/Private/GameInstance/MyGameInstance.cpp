// Copyright Xyzto Works


#include "GameInstance/MyGameInstance.h"
#include "GameInstance/GameContextManager.h"
#include "GameInstance/MySaveGame.h"
#include "GameMode/TowerFloorGameMode.h"
#include "Data/SaveSlotInfo.h"
#include "Level/LevelPortal.h"

#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "Characters/CharacterBase.h"
#include "Data/UnitDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"
#include "Party/PartyManagerSubsystem.h"
#include "Widgets/LoadingScreenWidget.h"
#include "World/FlagsSubsystem.h"
#include "World/NPCAffinitySubsystem.h"
#include "World/QuestSubsystem.h"
#include "World/WorldManagerSubsystem.h"


void UMyGameInstance::Init()
{
	Super::Init();

	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UMyGameInstance::HandlePostLoadMapWithWorld);

	// Do NOT create or load any save here.
	// The Main Menu is responsible for calling RequestContinue() or RequestNewGame().
	// Creating a save in Init() causes a phantom slot to appear before the player
	// has made any choice.
	GameContextManager = NewObject<UGameContextManager>(this);
	GameContextManager->InitializeSubsystem();

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
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	// Only commit and save if there is an active session to preserve.
	if (ActiveSave)
	{
		OnSaveRequested.Broadcast();
		CommitSave();
	}

	Super::Shutdown();
}

bool UMyGameInstance::HasSaveGame() const
{
	return !FindOccupiedSlotIndices().IsEmpty();
}
 
void UMyGameInstance::RequestNewGame()
{
	const int32 TargetSlot = AllocateNewSlotIndex();
	const FString SlotName = GetSlotNameForIndex(TargetSlot);

	// Flush subsystems to clean state BEFORE CreateNewSave commits to disk.
	// Without this, CommitSave inside CreateNewSave would bake stale subsystem
	// data (quests, flags, affinity) into the new save.
	if (UFlagsSubsystem* S = GetSubsystem<UFlagsSubsystem>())           S->Reset();
	if (UNPCAffinitySubsystem* S = GetSubsystem<UNPCAffinitySubsystem>()) S->Reset();
	if (UQuestSubsystem* S = GetSubsystem<UQuestSubsystem>())            S->Reset();

	CreateNewSave(SlotName);

	UE_LOG(LogTemp, Log,
		TEXT("[MyGameInstance] RequestNewGame: created new save in slot %d ('%s')"),
		TargetSlot, *SlotName);
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

FString UMyGameInstance::BuildSaveDisplayName() const
{
	// --- Level name ---
	FString LevelPart = TEXT("Unknown");
	if (UWorld* World = GetWorld())
	{
		// GetMapName() returns the full path — strip to just the map name
		LevelPart = World->GetMapName();
		LevelPart.RemoveFromStart(TEXT("UEDPIE_0_")); // strip PIE prefix if present
	}

	// --- Party leader: first active member ---
	FString LeaderName = TEXT("Unknown");
	int32   LeaderLevel = 1;

	for (const FPartyMemberData& Member : PartyData.Members)
	{
		if (!Member.bIsActivePartyMember) { continue; }
		if (!Member.CharacterClass)       { continue; }

		LeaderLevel = Member.Level;

		// Resolve display name from the UnitDefinition via CDO
		if (ACharacterBase* CDO = Member.CharacterClass->GetDefaultObject<ACharacterBase>())
		{
			if (UUnitStatsComponent* Stats = CDO->GetUnitStats())
			{
				if (const UUnitDefinition* Def = Stats->GetDefinition())
				{
					if (!Def->DisplayName.IsEmpty())
					{
						LeaderName = Def->DisplayName.ToString();
					}
				}
			}
		}
		break;
	}

	return FString::Printf(TEXT("%s - %s - Lv. %d"), *LevelPart, *LeaderName, LeaderLevel);
}

TArray<int32> UMyGameInstance::FindOccupiedSlotIndices() const
{
	TArray<int32> Result;
	int32 ConsecutiveMisses = 0;
	int32 i = 0;

	while (ConsecutiveMisses < MaxSlotScanGap)
	{
		if (UGameplayStatics::DoesSaveGameExist(GetSlotNameForIndex(i), 0))
		{
			Result.Add(i);
			ConsecutiveMisses = 0;
		}
		else
		{
			++ConsecutiveMisses;
		}
		++i;
	}

	return Result;
}

int32 UMyGameInstance::AllocateNewSlotIndex() const
{
	int32 i = 0;
	while (UGameplayStatics::DoesSaveGameExist(GetSlotNameForIndex(i), 0))
	{
		++i;
	}
	return i;
}

void UMyGameInstance::RequestContinue()
{
	const TArray<int32> OccupiedSlots = FindOccupiedSlotIndices();

	int32 MostRecentSlot = INDEX_NONE;
	FDateTime MostRecentTime = FDateTime::MinValue();

	for (int32 SlotIdx : OccupiedSlots)
	{
		UMySaveGame* SlotSave = Cast<UMySaveGame>(
			UGameplayStatics::LoadGameFromSlot(GetSlotNameForIndex(SlotIdx), 0));

		if (SlotSave && SlotSave->LastSavedAt > MostRecentTime)
		{
			MostRecentTime = SlotSave->LastSavedAt;
			MostRecentSlot = SlotIdx;
		}
	}

	if (MostRecentSlot == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MyGameInstance] RequestContinue: no valid save found."));
		return;
	}

	LoadSlotByIndex(MostRecentSlot);

	UE_LOG(LogTemp, Log,
		TEXT("[MyGameInstance] RequestContinue: loaded most recent save (slot %d)"), MostRecentSlot);
}

void UMyGameInstance::RequestSaveGame()
{
	if (!ActiveSave)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MyGameInstance] RequestSaveGame: no active save."));
		return;
	}

	ActiveSave->SaveDisplayName = BuildSaveDisplayName();
	ActiveSave->SavedLevelName  = GetSanitizedLevelName();

	OnSaveRequested.Broadcast();
	CommitSave();

	const bool bSuccess = UGameplayStatics::SaveGameToSlot(ActiveSave, ActiveSave->SaveSlotName, 0);
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("[MyGameInstance] RequestSaveGame: SaveGameToSlot failed."));
	}
}

void UMyGameInstance::RequestSaveToSlot(int32 SlotIndex)
{
    if (!ActiveSave)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[MyGameInstance] RequestSaveToSlot: no active save."));
        return;
    }

    const FString TargetSlotName = GetSlotNameForIndex(SlotIndex);

    // Collect current world state before snapshotting
    OnSaveRequested.Broadcast();

    // Build a snapshot of the current session state to write to the target slot.
    // We do NOT change ActiveSlotName or ActiveSave->SaveSlotName — the active
    // session continues on its original slot. This save is a copy-to operation.
    UMySaveGame* Snapshot = DuplicateObject<UMySaveGame>(ActiveSave, this);
    if (!Snapshot)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[MyGameInstance] RequestSaveToSlot: DuplicateObject failed."));
        return;
    }

    // Flush subsystems into GI caches so CommitSave has fresh data
    if (UFlagsSubsystem* S = GetSubsystem<UFlagsSubsystem>())             S->FlushToGameInstance();
    if (UNPCAffinitySubsystem* S = GetSubsystem<UNPCAffinitySubsystem>()) S->FlushToGameInstance();
    if (UQuestSubsystem* S = GetSubsystem<UQuestSubsystem>())              S->FlushToGameInstance();

    // Populate the snapshot with the current GI state
    Snapshot->SaveSlotName    = TargetSlotName;
    Snapshot->SaveDisplayName = BuildSaveDisplayName();
    Snapshot->SavedLevelName  = GetSanitizedLevelName();
    Snapshot->LastSavedAt     = FDateTime::UtcNow();
    Snapshot->PartyData          = PartyData;
    Snapshot->ProgressionData    = ProgressionData;
    Snapshot->GlobalFlags        = GlobalFlags;
    Snapshot->NPCAffinityMap     = NPCAffinityMap;
    Snapshot->QuestRuntimeStates = QuestRuntimeStates;
    // SavedActorsByLevel is already populated in ActiveSave via CollectWorldSaveData
    Snapshot->SavedActorsByLevel = ActiveSave->SavedActorsByLevel;

    const bool bSuccess = UGameplayStatics::SaveGameToSlot(Snapshot, TargetSlotName, 0);

    if (bSuccess)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[MyGameInstance] RequestSaveToSlot: saved copy to slot %d ('%s') as '%s'. Active slot unchanged ('%s')."),
            SlotIndex, *TargetSlotName, *Snapshot->SaveDisplayName, *ActiveSlotName);
    }
    else
    {
        UE_LOG(LogTemp, Error,
            TEXT("[MyGameInstance] RequestSaveToSlot: SaveGameToSlot failed for slot %d"), SlotIndex);
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

	for (const FSaveSlotInfo& Info : GetAllSaveSlots())
	{
		UMySaveGame* SlotSave = Cast<UMySaveGame>(
			UGameplayStatics::LoadGameFromSlot(Info.SlotName, 0));

		if (SlotSave && SlotSave->LastSavedAt > MostRecentTime)
		{
			MostRecentTime = SlotSave->LastSavedAt;
			BestInfo       = Info;
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
	const TArray<int32> OccupiedSlots = FindOccupiedSlotIndices();

	for (int32 SlotIdx : OccupiedSlots)
	{
		FSaveSlotInfo Info;
		Info.SlotIndex   = SlotIdx;
		Info.SlotName    = GetSlotNameForIndex(SlotIdx);
		Info.bIsOccupied = true;

		UMySaveGame* SlotSave = Cast<UMySaveGame>(
			UGameplayStatics::LoadGameFromSlot(Info.SlotName, 0));

		if (SlotSave)
		{
			Info.DisplayName = SlotSave->SaveDisplayName.IsEmpty()
				? FString::Printf(TEXT("Save %d"), SlotIdx + 1)
				: SlotSave->SaveDisplayName;

			Info.SavedLevelName = SlotSave->SavedLevelName;

			const int32 TotalMinutes = FMath::FloorToInt(SlotSave->PlaytimeSeconds / 60.f);
			Info.PlaytimeFormatted   = FString::Printf(TEXT("%dh %dm"),
				TotalMinutes / 60, TotalMinutes % 60);

			const FTimespan Delta  = FDateTime::UtcNow() - SlotSave->LastSavedAt;
			const int32 MinutesAgo = FMath::FloorToInt(Delta.GetTotalMinutes());
			const int32 HoursAgo   = FMath::FloorToInt(Delta.GetTotalHours());
			const int32 DaysAgo    = FMath::FloorToInt(Delta.GetTotalDays());

			if (MinutesAgo < 1)
				Info.LastSavedFormatted = TEXT("Just now");
			else if (MinutesAgo < 60)
				Info.LastSavedFormatted = FString::Printf(TEXT("%d min ago"), MinutesAgo);
			else if (HoursAgo < 24)
				Info.LastSavedFormatted = FString::Printf(TEXT("%dh ago"), HoursAgo);
			else
				Info.LastSavedFormatted = FString::Printf(TEXT("%d day%s ago"),
					DaysAgo, DaysAgo == 1 ? TEXT("") : TEXT("s"));
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
	ActiveSave         = nullptr;
	PartyData          = FPartyData();
	ProgressionData    = FProgressionData();
	GlobalFlags.Empty();
	NPCAffinityMap.Empty();
	QuestRuntimeStates.Empty();
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
	if (!ActiveSave) { return; }

	// Flush all subsystems into GI caches first
	if (UFlagsSubsystem* S = GetSubsystem<UFlagsSubsystem>())             S->FlushToGameInstance();
	if (UNPCAffinitySubsystem* S = GetSubsystem<UNPCAffinitySubsystem>()) S->FlushToGameInstance();
	if (UQuestSubsystem* S = GetSubsystem<UQuestSubsystem>())              S->FlushToGameInstance();

	// Copy all GI caches into ActiveSave atomically
	ActiveSave->PartyData          = PartyData;
	ActiveSave->ProgressionData    = ProgressionData;
	ActiveSave->GlobalFlags        = GlobalFlags;
	ActiveSave->NPCAffinityMap     = NPCAffinityMap;
	ActiveSave->QuestRuntimeStates = QuestRuntimeStates;
}

bool UMyGameInstance::LoadSaveSlot(const FString& SlotName)
{
	ActiveSave = Cast<UMySaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!ActiveSave) { return false; }

	ActiveSlotName       = SlotName;
	PartyData            = ActiveSave->PartyData;
	ProgressionData      = ActiveSave->ProgressionData;
	GlobalFlags          = ActiveSave->GlobalFlags;
	NPCAffinityMap       = ActiveSave->NPCAffinityMap;
	QuestRuntimeStates   = ActiveSave->QuestRuntimeStates;

	if (UPartyManagerSubsystem* PartyMgr = GetSubsystem<UPartyManagerSubsystem>())
		PartyMgr->SyncFromGameInstance();

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

    // Only update SavedLevelName when leaving a gameplay level (TowerFloorGameMode).
    // When leaving the MainMenu, preserving the previously saved level name ensures
    // that Continue always brings the player back to where they actually played,
    // not to the MainMenu itself.
    if (WorldToSave->GetAuthGameMode() &&
        WorldToSave->GetAuthGameMode()->IsA<ATowerFloorGameMode>())
    {
        ActiveSave->SavedLevelName = GetSanitizedLevelName();

        UE_LOG(LogTemp, Log,
            TEXT("[MyGameInstance] AutoSaveBeforeTravel: updating SavedLevelName to '%s'"),
            *ActiveSave->SavedLevelName.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Log,
            TEXT("[MyGameInstance] AutoSaveBeforeTravel: non-gameplay world ('%s') — preserving SavedLevelName '%s'"),
            *WorldToSave->GetName(), *ActiveSave->SavedLevelName.ToString());
    }

    UE_LOG(LogTemp, Warning, TEXT("[AutoSave] Collecting from world: %s"), *WorldToSave->GetName());

    if (UWorldManagerSubsystem* WorldMgr = WorldToSave->GetSubsystem<UWorldManagerSubsystem>())
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoSave] Got subsystem, calling CollectWorldSaveData"));
        WorldMgr->CollectWorldSaveData();
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[AutoSave] No WorldManagerSubsystem found in world '%s'!"), *WorldToSave->GetName());
    }

    CommitSave();

    const bool bSuccess = UGameplayStatics::SaveGameToSlot(ActiveSave, ActiveSave->SaveSlotName, 0);

    if (bSuccess)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[MyGameInstance] AutoSaveBeforeTravel: saved to slot '%s' from world '%s'"),
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

FName UMyGameInstance::GetSanitizedLevelNameForWorld(UWorld* InWorld) const
{
	if (!InWorld) { return NAME_None; }

	FString MapName = InWorld->GetMapName();
	MapName.RemoveFromStart(TEXT("UEDPIE_0_"));
	MapName.RemoveFromStart(TEXT("UEDPIE_1_"));
	MapName.RemoveFromStart(TEXT("UEDPIE_2_"));

	return FName(*MapName);
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

FName UMyGameInstance::GetSanitizedLevelName() const
{
	return GetSanitizedLevelNameForWorld(GetWorld());
}


void UMyGameInstance::Debug_ResetProgression()
{
	ProgressionData = FProgressionData();
}