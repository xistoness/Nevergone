// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Audio/AudioCategory.h"
#include "Audio/AudioSubsystem.h"
#include "Engine/GameInstance.h"
#include "Data/PartyData.h"
#include "Data/ProgressionData.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundMix.h"
#include "Types/LevelTypes.h"
#include "MyGameInstance.generated.h"

class ACharacterPreviewStudio;
class UStatusEffectDefinition;
class USoundClass;
class USoundAttenuation;
struct FActorSaveData;
class UMySaveGame;
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

	// Returns true if this level has already had a first-visit snapshot taken.
	// Used by RestoreWorldState to skip the restore pipeline on first entry.
	bool HasLevelBeenVisited(FName LevelName) const;

	// Marks the level as visited so subsequent loads run the full restore pipeline.
	// Called by WorldManagerSubsystem immediately after the first-visit snapshot.
	void MarkLevelVisited(FName LevelName) const;
	
	// Level Transition
	void RequestLevelChange(FName TargetLevel, const FLevelTransitionContext& Context);
	
	// Callbacks
	void OnPreLevelUnload() const;
	void OnPostLevelLoad() const;
	
	// Called after the level has successfully loaded
	void ApplyPersistentStateToWorld(UWorld* World) const;

	// Applies the pending portal entry point to the current pawn.
	// Called by TowerFloorGameMode after the exploration pawn is successfully
	// possessed, guaranteeing the pawn exists when teleportation runs.
	// bHasPendingEntryPoint is cleared on the first call so subsequent loads
	// are unaffected.
	void ApplyPendingEntryPoint(UWorld* World);
	
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

	// Automation only. While set, RequestNewGame writes to this slot name instead of a
	// numbered campaign slot, so test runs never show up in the player's slot list.
	// An empty name (the default) restores the normal behaviour.
	void SetAutomationSlotName(const FString& SlotName) { AutomationSlotName = SlotName; }

	// Automation only. Ends the test session: drops the active save, clears the slot
	// name and deletes the test slot's file.
	void EndAutomationSession();

	// Automation only. Deletes the save file of the given slot name, if any.
	static void DeleteAutomationSlot(const FString& SlotName);
 
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

	/**
	 * Attenuation for world sounds (SFX at a location or attached) whose asset
	 * has none of its own, so no world sound plays at the same volume at any
	 * distance. UI sounds and music never get it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|SFX|Defaults")
	TObjectPtr<USoundAttenuation> DefaultWorldAttenuation;

	/** Active Sound Mix used for per-category volume control. */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|Mixing")
	TObjectPtr<USoundMix> MasterSoundMix;

	/**
	 * USoundClass asset for each player-facing audio category.
	 * Authored once in BP_MyGameInstance Class Defaults; mirrored to
	 * UAudioSubsystem at Init time. Without these mappings, the volume
	 * sliders in the Settings tab will not affect the live mix.
	 *
	 * Recommended layout:
	 *   Master         → SC_Master  (parent of all the others)
	 *   Music          → SC_Music   (child of SC_Master)
	 *   SFX            → SC_SFX     (child of SC_Master)
	 *   UI             → SC_UI      (child of SC_Master)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|Mixing")
	TMap<ENevergoneAudioCategory, TObjectPtr<USoundClass>> CategorySoundClasses;

	/** Per-event UI sound map. Assign assets once — widgets call PlayUISoundEvent() by enum. */
	UPROPERTY(EditDefaultsOnly, Category = "Audio|UI")
	TMap<EUISoundEvent, TObjectPtr<USoundBase>> UISoundMap;

	/** Status effect applied to all active party members when carrying weight exceeds MaxWeight.
	 *  Assigned here because UGameInstanceSubsystem properties cannot be set via Class Defaults.
	 *  InventorySubsystem reads this during Initialize() via GetGameInstance<UMyGameInstance>().
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSoftObjectPtr<UStatusEffectDefinition> OverweightStatusDefinition;
	
	UPROPERTY(EditAnywhere, Category = "Preview")
	TSubclassOf<ACharacterPreviewStudio> CharacterPreviewStudio;

	// -----------------------------------------------------------------------
	// Localization
	// -----------------------------------------------------------------------
	// Read by ULocalizationSubsystem during its Initialize() to know which
	// cultures the project ships with. Assigned here (and not on the
	// subsystem) because UGameInstanceSubsystem UPROPERTY defaults are not
	// editable via Class Defaults — same pattern used by OverweightStatusDefinition.
	//
	// Use ICU/Unreal codes: "en", "pt-BR", "es", "fr", "ja", etc.
	// To add a new language later, add the code here AND ship a compiled
	// .locres for it via the Localization Dashboard.

	/** Cultures that ship compiled .locres files. Edit in the BP class defaults of UMyGameInstance. */
	UPROPERTY(EditDefaultsOnly, Category = "Localization")
	TArray<FString> SupportedCultures = { TEXT("en"), TEXT("pt-BR") };

	/**
	 * Culture used when the persisted preference and the OS culture both fail
	 * to match anything in SupportedCultures. Should always be one of the
	 * codes listed above — typically the project's native authoring language.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Localization")
	FString FallbackCulture = TEXT("en");

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

	// Set only by automation; see SetAutomationSlotName.
	FString AutomationSlotName;

	UPROPERTY()
	FPartyData PartyData;

	UPROPERTY()
	FProgressionData ProgressionData;
	
	bool bDidInitialWorldRestore = false;
	
	// True when a level transition has a portal entry point waiting to be applied.
	// Cleared by ApplyPendingEntryPoint() on first call.
	bool bHasPendingEntryPoint = false;
	
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
