// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Audio/AudioSubsystem.h"
#include "Engine/GameInstance.h"
#include "Data/PartyData.h"
#include "Data/ProgressionData.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundMix.h"
#include "Types/LevelTypes.h"
#include "MyGameInstance.generated.h"

struct FActorSaveData;
class UMySaveGame;
class UGameContextManager;
class ULoadingScreenWidget;


UENUM()
enum class ELevelTransitionState : uint8
{
	Idle,
	Transitioning
};


UCLASS()
class NEVERGONE_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	
	// Returns true if a save file exists in the default slot
	UFUNCTION(BlueprintCallable)
	bool HasSaveGame() const;
 
	// Starts a fresh playthrough — wipes the existing slot and opens the first level
	UFUNCTION(BlueprintCallable)
	void RequestNewGame();
 
	// Loads the existing save and opens the saved level
	UFUNCTION(BlueprintCallable)
	void RequestContinue();
	
	UFUNCTION(BlueprintCallable)
	void RequestSaveGame();
	
	// Saves to a specific slot by index.
	// If the slot is empty, creates a new save there.
	// Updates ActiveSlotName so subsequent saves go to the same slot.
	UFUNCTION(BlueprintCallable, Category = "Save")
	void RequestSaveToSlot(int32 SlotIndex);
	
	void CommitSave() const;
	
	// Loads the save from disk and applies it to the current world
	UFUNCTION(BlueprintCallable)
	void RequestLoadGame();
	
	UFUNCTION(BlueprintCallable, Category = "Save")
	FSaveSlotInfo GetMostRecentSaveInfo() const;
	
	// Applies current ActiveSave to the world (no disk I/O)
	UFUNCTION(BlueprintCallable)
	void ApplyActiveSaveToWorld();
	
	UGameContextManager* GetGameContextManager() const;
	
	// TODO
	// Party
	const FPartyData& GetPartyData() const;
	void SetPartyData(const FPartyData& NewData);
	
	// Progression
	const FProgressionData& GetProgressionData() const;
	void SetProgressionData(const FProgressionData& NewData);
		
	// Global Flags
	bool GetGlobalFlag(FName Flag) const;
	void SetGlobalFlag(FName Flag, bool bValue);
	
	// Saved Actors
	const TArray<FActorSaveData>& GetSavedActorsForLevel(FName LevelName) const;
	void SetSavedActorsForLevel(FName LevelName, const TArray<FActorSaveData>& Actors) const;
	
	// Level Transition
	void RequestLevelChange(FName TargetLevel, const FLevelTransitionContext& Context);
	
	// Callbacks
	void OnPreLevelUnload() const;
	void OnPostLevelLoad() const;
	
	// Called after the level has successfully loaded
	void ApplyPersistentStateToWorld(UWorld* World) const;
	
	// Total number of save slots available to the player
	UPROPERTY(EditDefaultsOnly, Category = "Save")
	int32 MaxSaveSlots = 3;
 
	// Returns metadata for all slots (occupied and empty) for the Load Game panel
	UFUNCTION(BlueprintCallable, Category = "Save")
	TArray<FSaveSlotInfo> GetAllSaveSlots() const;
 
	// Load a specific slot by index (used by Load Game panel)
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadSlotByIndex(int32 SlotIndex);
 
	// Delete a specific slot by index
	UFUNCTION(BlueprintCallable, Category = "Save")
	void DeleteSlotByIndex(int32 SlotIndex);
 
	// Returns the slot name string for a given index
	UFUNCTION(BlueprintCallable, Category = "Save")
	FString GetSlotNameForIndex(int32 SlotIndex) const;
 
	// Updates the active save's display metadata before writing to disk
	// Call this right before RequestSaveGame() to keep the slot info fresh
	UFUNCTION(BlueprintCallable, Category = "Save")
	void UpdateSaveMetadata(const FString& DisplayName, FName CurrentLevelName);
	
	UFUNCTION(BlueprintCallable, Category = "Save")
	FName GetActiveSavedLevelName() const;
	
	UMySaveGame* GetActiveSave() const;
	
	// Public variant used by GameContextManager (which has its own World reference)
	FName GetSanitizedLevelNameForWorld(UWorld* InWorld) const;
	
	// Setters called by subsystem FlushToGameInstance()
	void SetNPCAffinityMap(const TMap<FGuid, int32>& InMap) { NPCAffinityMap = InMap; }
	void SetQuestRuntimeStates(const TMap<FName, FQuestRuntimeState>& InStates) { QuestRuntimeStates = InStates; }
	
	// Event Broadcasting
	//OnRestoreParty.Broadcast(PartyData);
	//OnRestoreProgress.Broadcast(ProgressionData);
	
	DECLARE_MULTICAST_DELEGATE(FOnSaveRequested);
	DECLARE_MULTICAST_DELEGATE(FOnSaveLoaded);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartyChanged, const FPartyData&);

	FOnSaveLoaded OnSaveLoaded;
	FOnPartyChanged OnPartyChanged;
	FOnSaveRequested OnSaveRequested;
	
	UPROPERTY()
	TMap<FName, bool> GlobalFlags;
	
	// Intermediate runtime caches — mirrors of what subsystems manage.
	// CommitSave copies these into ActiveSave atomically.
	UPROPERTY()
	TMap<FGuid, int32> NPCAffinityMap;

	UPROPERTY()
	TMap<FName, FQuestRuntimeState> QuestRuntimeStates;
	
	
	// Debugging
	void Debug_PrintGlobalState() const;
	void Debug_ResetProgression();
	
	/** Fallback SFX fired when an ability damage event has no ImpactSound. */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|SFX|Defaults")
	TObjectPtr<USoundBase> DefaultHitSFX;

	/** Fallback SFX fired when a unit dies with no death sound assigned. */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|SFX|Defaults")
	TObjectPtr<USoundBase> DefaultDeathSFX;

	/** Fallback SFX fired when a heal is applied with no heal sound assigned. */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|SFX|Defaults")
	TObjectPtr<USoundBase> DefaultHealSFX;

	/** Active Sound Mix used for per-category volume control. */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|Mixing")
	TObjectPtr<USoundMix> MasterSoundMix;

	/** Per-event UI sound map. Assign assets once — widgets call PlayUISoundEvent() by enum. */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|UI")
	TMap<EUISoundEvent, TObjectPtr<USoundBase>> UISoundMap;

protected:
	
	virtual void Init() override;
	virtual void Shutdown() override;
	
	void SetActiveSave(UMySaveGame* Save);
	
	bool LoadSaveSlot(const FString& SlotName);
	bool CreateNewSave(const FString& SlotName);
	void ClearActiveSave();
	
	
	// LEVEL TRANSITION // 
	// Called right before leaving the current map
	void BeginLevelTransition();

	// Called after a new map is loaded (PostLoadMapWithWorld)
	void EndLevelTransition(UWorld* LoadedWorld);

	// Delegate callback
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	// UI helpers
	void ShowLoadingScreen();
	void HideLoadingScreen();

	// Input helpers
	void SetInputLocked(bool bLocked);

	// Save helpers
	void AutoSaveBeforeTravel(UWorld* WorldToSave);
	
private:
	UPROPERTY()
	UMySaveGame* ActiveSave;
	
	UPROPERTY()
	FString ActiveSlotName;

	UPROPERTY()
	FPartyData PartyData;

	UPROPERTY()
	FProgressionData ProgressionData;
	
	UPROPERTY()
	UGameContextManager* GameContextManager;
	
	bool bDidInitialWorldRestore = false;
	
	bool bHasPendingEntryPoint = false;

	void ApplyPendingEntryPoint(UWorld* World);
	
	int32 FindNextAvailableSlot() const;
	
	// Builds the save display name from the current level and party leader.
	// Format: "LevelName - LeaderName - Lv. X"
	FString BuildSaveDisplayName() const;
	
	// Returns all slot indices that have a save file on disk.
	// Scans sequentially until MaxScanIndex consecutive misses.
	TArray<int32> FindOccupiedSlotIndices() const;

	// Returns the next slot index that does not exist on disk.
	int32 AllocateNewSlotIndex() const;
	
	FName GetSanitizedLevelName() const;

	static constexpr int32 MaxSlotScanGap = 10; // stop scanning after 10 consecutive misses
	
	// LEVEL TRANSITION // 
	
	UPROPERTY()
	ULoadingScreenWidget* LoadingWidgetInstance = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Loading")
	TSubclassOf<ULoadingScreenWidget> LoadingWidgetClass;

	ELevelTransitionState TransitionState = ELevelTransitionState::Idle;

	// Keep your existing pending context
	FLevelTransitionContext PendingLevelTransition;
};