// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Audio/AudioCategory.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameInstance/GameContextManager.h"
#include "AudioSubsystem.generated.h"

class USoundBase;
class USoundClass;
class USoundMix;
class UAudioComponent;
class UCombatEventBus;
class ACharacterBase;

// ---------------------------------------------------------------------------
// Music state enum
//
// Tracks the logical context of the music currently playing.
// Does NOT determine which asset plays — that is the caller's responsibility.
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EMusicState : uint8
{
	None,
	MainMenu,
	Exploration,
	Battle,
	BattleIntense,  // Low HP, boss phase — caller switches to this when needed
	Victory,
	Defeat
};

// ---------------------------------------------------------------------------
// UI sound event types — used to look up mapped assets without widgets
// knowing which USoundBase belongs to each action.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EUISoundEvent : uint8
{
	ButtonConfirm,     // Generic confirm / select
	ButtonCancel,    // Back / cancel
	ButtonHover,     // Cursor moved onto a button
	TabSwitch,       // Switched between tabs or panels
	AbilityUseInvalid,   // Player tried to use a locked/cooldown ability
};

// ---------------------------------------------------------------------------

/**
 * Central audio subsystem for Nevergone.
 *
 * Lives on UGameInstance for the entire session — survives level transitions.
 * This is the ONLY place in the codebase that calls audio playback directly.
 *
 * Design principle — this subsystem is a pure EXECUTOR:
 *   It does not know which music track belongs to which level.
 *   The caller (GameMode, Controller) decides the asset and passes it in.
 *   This makes per-floor, per-boss, per-scene music trivially easy to support.
 *
 * Typical callers:
 *   TowerFloorGameMode  → PlayMusic(ExplorationMusic, EMusicState::Exploration)
 *                         PlayMusic(BattleMusic,      EMusicState::Battle)
 *   MainMenuController  → PlayMusic(MenuMusic,        EMusicState::MainMenu)
 *
 * SFX fallbacks (DefaultHitSFX, DefaultDeathSFX, DefaultHealSFX) and
 * MasterSoundMix are the only assets that live here, because they are
 * project-wide and not level-specific.
 *
 * Migration path for animations:
 *   When AnimNotify_PlaySFX is added, it calls this subsystem exactly as
 *   abilities do now — no changes needed here.
 */
UCLASS()
class NEVERGONE_API UAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	// -----------------------------------------------------------------------
	// UGameInstanceSubsystem interface
	// -----------------------------------------------------------------------

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// -----------------------------------------------------------------------
	// Music API
	// -----------------------------------------------------------------------

	/**
	 * Plays a music track with crossfade.
	 * The caller provides the asset — this subsystem handles the fade lifecycle.
	 * If the same track is already playing, the call is a no-op.
	 *
	 * @param Track        Asset to play. Pass null to stop music.
	 * @param NewState     Logical context (for tracking / future ducking logic)
	 * @param FadeOutTime  Seconds to fade out the current track
	 * @param FadeInTime   Seconds to fade in the new track
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	void PlayMusic(USoundBase* Track, EMusicState NewState, float FadeOutTime = 1.0f, float FadeInTime = 1.0f);

	/** Stops music with a fade. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	void StopMusic(float FadeOutTime = 1.0f);

	/** Returns the current music state — useful for conditional logic in callers. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	EMusicState GetCurrentMusicState() const { return CurrentMusicState; }

	// -----------------------------------------------------------------------
	// SFX API — world-positioned (3D attenuation)
	// -----------------------------------------------------------------------

	/**
	 * Plays a one-shot SFX at a world location.
	 * Use for: attack impacts, ability effects, footsteps.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|SFX")
	void PlaySFXAtLocation(USoundBase* Sound, FVector Location,
		float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	/**
	 * Plays a one-shot SFX attached to a scene component.
	 * Use for: character-bound loops, projectiles that follow their owner.
	 * Returns the playing component (auto-destroyed when it finishes) so the
	 * caller can stop or pause a loop, or null when nothing played.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|SFX")
	UAudioComponent* PlaySFXAttached(USoundBase* Sound, USceneComponent* AttachTo,
		float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	// -----------------------------------------------------------------------
	// SFX API — 2D (no attenuation)
	// -----------------------------------------------------------------------

	/**
	 * Plays a screen-space sound with no spatial attenuation.
	 * Use for: button clicks, confirmations, turn start indicators.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|UI")
	void PlayUISound(USoundBase* Sound);
	
	/** Plays a UI sound by event type, resolved against the UISoundMap. */
	UFUNCTION(BlueprintCallable, Category = "Audio|UI")
	void PlayUISoundEvent(EUISoundEvent Event);


	// -----------------------------------------------------------------------
	// Volume control — low-level
	// -----------------------------------------------------------------------

	/**
	 * Adjusts the volume of a Sound Class via the active Sound Mix.
	 * @param SoundClass  Target Sound Class asset
	 * @param Volume      Normalized volume (0.0–1.0)
	 * @param FadeTime    Blend duration in seconds
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Volume")
	void SetSoundClassVolume(USoundClass* SoundClass, float Volume, float FadeTime = 0.0f);

	// -----------------------------------------------------------------------
	// Volume control — category API
	//
	// Player-facing volume sliders go through this layer. It maps the
	// logical category to its USoundClass (via CategorySoundClasses), applies
	// the volume to the running mix, and persists the new value into
	// UNevergoneGameUserSettings so it survives the session.
	// -----------------------------------------------------------------------

	/**
	 * Returns the persisted volume for a category, in [0..1].
	 * Reads from UNevergoneGameUserSettings — single source of truth for the value.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Audio|Volume")
	float GetCategoryVolume(ENevergoneAudioCategory Category) const;

	/**
	 * Sets the volume for a category, persisting the choice and applying it
	 * to the live SoundMix. Volume is clamped to [0..1].
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Volume")
	void SetCategoryVolume(ENevergoneAudioCategory Category, float NewVolume);

	/**
	 * Re-applies all category volumes (Master/Music/SFX/UI) from
	 * UNevergoneGameUserSettings to the live SoundMix.
	 * Called once during Initialize so freshly booted sessions hear the
	 * player's saved levels rather than asset defaults.
	 */
	void ApplyAllVolumesFromSettings();

	// -----------------------------------------------------------------------
	// Combat integration
	// -----------------------------------------------------------------------

	/**
	 * Binds this subsystem to a CombatEventBus for the duration of a battle.
	 * Combat SFX (hits, deaths, heals) will fire automatically.
	 * Called by UCombatManager::Initialize after creating the event bus.
	 */
	void BindToCombatEventBus(UCombatEventBus* EventBus);

	// -----------------------------------------------------------------------
	// Global SFX fallbacks — project-wide defaults, NOT per-level.
	// Assign once in the GameInstance Blueprint.
	// -----------------------------------------------------------------------

	UPROPERTY()
	TObjectPtr<USoundBase> DefaultHitSFX;

	UPROPERTY()
	TObjectPtr<USoundBase> DefaultDeathSFX;

	UPROPERTY()
	TObjectPtr<USoundBase> DefaultHealSFX;

	UPROPERTY()
	TObjectPtr<USoundMix> MasterSoundMix;

	UPROPERTY()
	TMap<EUISoundEvent, TObjectPtr<USoundBase>> UISoundMap;

	/**
	 * Mapping of logical audio categories to their USoundClass assets.
	 * Populated by UMyGameInstance::Init from its own mirror UPROPERTY so the
	 * project Blueprint can author the assignment in Class Defaults.
	 */
	UPROPERTY()
	TMap<ENevergoneAudioCategory, TObjectPtr<USoundClass>> CategorySoundClasses;

private:

	// -----------------------------------------------------------------------
	// Context change handler
	// -----------------------------------------------------------------------

	/**
	 * Subscribed to UGameContextManager::OnGameContextChanged.
	 * Only handles the Transition state (fade out before level travel).
	 * All other music changes are driven by TowerFloorGameMode directly.
	 */
	void HandleGameContextChanged(EGameContextState NewState);

	/**
	 * Subscribed to FCoreUObjectDelegates::PostLoadMapWithWorld.
	 * Triggers a re-apply of category volumes once the world's audio device
	 * is fully initialized — this is the earliest reliable point where
	 * SoundMix push and class overrides actually take audible effect.
	 */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	// -----------------------------------------------------------------------
	// CombatEventBus handlers
	// -----------------------------------------------------------------------

	void HandleDamageApplied(ACharacterBase* Source, ACharacterBase* Target, int32 Amount);
	void HandleUnitDied(ACharacterBase* Unit);
	void HandleHealApplied(ACharacterBase* Source, ACharacterBase* Target, int32 Amount);

	// -----------------------------------------------------------------------
	// Internal
	// -----------------------------------------------------------------------

	/** Core playback: fades in a track on the persistent music component. */
	void PlayMusicTrack(USoundBase* Track, float FadeInTime);

	// -----------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------

	/** Logical context of the music currently playing. */
	EMusicState CurrentMusicState = EMusicState::None;

	/**
	 * The asset currently playing (or about to play after a fade).
	 * Compared in PlayMusic to avoid restarting the same track.
	 */
	UPROPERTY()
	TObjectPtr<USoundBase> CurrentMusicTrack;

	/**
	 * Persistent UAudioComponent for music.
	 * Created once on the first PlayMusic call.
	 * bStopWhenOwnerDestroyed=false so it survives level transitions.
	 */
	UPROPERTY()
	TObjectPtr<UAudioComponent> MusicComponent;

	/** Weak ref to the active CombatEventBus — auto-cleared when combat ends. */
	TWeakObjectPtr<UCombatEventBus> BoundEventBus;

	/** GameContextManager delegate handle — stored for clean removal. */
	FDelegateHandle ContextChangedHandle;

	/** PostLoadMapWithWorld delegate handle — stored for clean removal. */
	FDelegateHandle PostLoadMapHandle;

	/** Deferred fade-in timer — fires after fade-out completes. */
	FTimerHandle MusicFadeInHandle;

	/**
	 * Tracks whether MasterSoundMix has been pushed onto the audio device.
	 * SoundMix overrides only take effect while the mix is active (pushed) —
	 * we push lazily on the first ApplyAllVolumesFromSettings (when both World
	 * and MasterSoundMix are guaranteed valid) and pop on Deinitialize.
	 */
	bool bMasterMixPushed = false;
	
};
