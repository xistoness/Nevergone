// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
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
	void PlaySFXAtLocation(USoundBase* Sound, FVector Location);

	/**
	 * Plays a one-shot SFX attached to a scene component.
	 * Use for: character-bound loops, projectiles that follow their owner.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|SFX")
	void PlaySFXAttached(USoundBase* Sound, USceneComponent* AttachTo);

	// -----------------------------------------------------------------------
	// SFX API — 2D (no attenuation)
	// -----------------------------------------------------------------------

	/**
	 * Plays a screen-space sound with no spatial attenuation.
	 * Use for: button clicks, confirmations, turn start indicators.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|UI")
	void PlayUISound(USoundBase* Sound);

	// -----------------------------------------------------------------------
	// Volume control
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

	/** Fires via EventBus when an ability damage event has no ImpactSound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Audio|SFX|Defaults")
	TObjectPtr<USoundBase> DefaultHitSFX;

	/** Fires via EventBus when a unit dies and no death SFX is set. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Audio|SFX|Defaults")
	TObjectPtr<USoundBase> DefaultDeathSFX;

	/** Fires via EventBus when a heal is applied and no heal SFX is set. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Audio|SFX|Defaults")
	TObjectPtr<USoundBase> DefaultHealSFX;

	// -----------------------------------------------------------------------
	// Sound Mix — global, assign once in the GameInstance Blueprint.
	// -----------------------------------------------------------------------

	/** Active Sound Mix used for per-category volume control. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Audio|Mixing")
	TObjectPtr<USoundMix> MasterSoundMix;

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

	/** Deferred fade-in timer — fires after fade-out completes. */
	FTimerHandle MusicFadeInHandle;
};