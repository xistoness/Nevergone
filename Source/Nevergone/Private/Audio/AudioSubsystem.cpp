// Copyright Xyzto Works

#include "Audio/AudioSubsystem.h"

#include "GameInstance/GameContextManager.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "Characters/CharacterBase.h"
#include "Localization/NevergoneGameUserSettings.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

// ---------------------------------------------------------------------------
// UGameInstanceSubsystem interface
// ---------------------------------------------------------------------------

void UAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The subsystem no longer auto-transitions music on context change.
	// TowerFloorGameMode calls PlayMusic() directly with the correct asset.
	// We still subscribe here in case future systems need the hook.
	if (UGameContextManager* ContextManager = GetGameInstance()->GetSubsystem<UGameContextManager>())
	{
		ContextChangedHandle = ContextManager->OnGameContextChanged.AddUObject(
			this,
			&UAudioSubsystem::HandleGameContextChanged
		);

		UE_LOG(LogTemp, Log, TEXT("[Audio] AudioSubsystem initialized."));
	}

	// Apply persisted player volumes to the live SoundMix.
	// Note on ordering: UMyGameInstance::Init populates CategorySoundClasses
	// BEFORE the subsystems' Initialize() is called for new subsystems but
	// not necessarily for already-instantiated ones. To be safe, the GI also
	// calls ApplyAllVolumesFromSettings explicitly after the assignments —
	// this first call here is mostly for editor PIE startups where the GI
	// path still applies but we want a sensible initial state.
	ApplyAllVolumesFromSettings();

	// Subscribe to PostLoadMapWithWorld so volumes are re-applied once the
	// audio device is fully created and bound to a world.
	//
	// Why this is necessary (junior-friendly):
	//   UGameInstance::Init() — and therefore this subsystem's Initialize() and
	//   the first ApplyAllVolumesFromSettings call from MyGameInstance — both
	//   run BEFORE the engine creates the level's audio device. SoundMix
	//   overrides registered before the device exists are silently dropped by
	//   the audio thread because there is no live mix bus to apply them to.
	//   That's why "raise the slider then lower it again" worked while the
	//   initial 0% on launch did not — the first state was applied too early.
	//
	//   PostLoadMapWithWorld fires AFTER the world's audio device is up and
	//   running, which is the earliest point where push + override are
	//   guaranteed to take effect.
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UAudioSubsystem::HandlePostLoadMap);
}

void UAudioSubsystem::Deinitialize()
{
	// Unsubscribe from PostLoadMapWithWorld first so a level transition during
	// shutdown cannot trigger a re-apply on a half-torn-down subsystem.
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	PostLoadMapHandle.Reset();

	// Pop the master mix if we pushed it. PopSoundMixModifier is no-op if the
	// mix was never pushed; we still gate on bMasterMixPushed so the log is accurate.
	if (bMasterMixPushed)
	{
		if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		{
			if (MasterSoundMix)
			{
				UGameplayStatics::PopSoundMixModifier(World, MasterSoundMix);

				UE_LOG(LogTemp, Log,
					TEXT("[Audio] Deinitialize: popped MasterSoundMix '%s' from audio device"),
					*GetNameSafe(MasterSoundMix));
			}
		}
		bMasterMixPushed = false;
	}

	// Remove GameContextManager binding
	if (UGameContextManager* ContextManager = GetGameInstance()->GetSubsystem<UGameContextManager>())
	{
		ContextManager->OnGameContextChanged.Remove(ContextChangedHandle);
	}
	
	// Remove GameContextManager binding
	if (UGameContextManager* ContextManager = GetGameInstance()->GetSubsystem<UGameContextManager>())
	{
		ContextManager->OnGameContextChanged.Remove(ContextChangedHandle);
	}

	// Remove CombatEventBus bindings
	if (UCombatEventBus* EventBus = BoundEventBus.Get())
	{
		EventBus->OnDamageApplied.RemoveAll(this);
		EventBus->OnUnitDied.RemoveAll(this);
		EventBus->OnHealApplied.RemoveAll(this);
	}

	// Stop and release music
	if (MusicComponent)
	{
		MusicComponent->Stop();
		MusicComponent = nullptr;
	}

	// Clear the deferred fade-in if it's pending
	if (UWorld* World = GetGameInstance()->GetWorld())
	{
		World->GetTimerManager().ClearTimer(MusicFadeInHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("[Audio] AudioSubsystem deinitialized."));

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Music API
// ---------------------------------------------------------------------------

void UAudioSubsystem::PlayMusic(USoundBase* Track, EMusicState NewState, float FadeOutTime, float FadeInTime)
{
	// Null track = explicit stop
	if (!Track)
	{
		StopMusic(FadeOutTime);
		return;
	}

	// Skip if the same track is already playing in the same state —
	// avoids restarting music when returning from battle to the same floor
	if (Track == CurrentMusicTrack && CurrentMusicState == NewState)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Audio] PlayMusic: '%s' is already playing — skipped."), *GetNameSafe(Track));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Audio] PlayMusic: '%s' | State: %d -> %d (FadeOut=%.1fs, FadeIn=%.1fs)"),
		*GetNameSafe(Track), (int32)CurrentMusicState, (int32)NewState, FadeOutTime, FadeInTime);

	CurrentMusicState = NewState;
	CurrentMusicTrack = Track;

	// Cancel any pending deferred fade-in from a previous transition
	if (UWorld* World = GetGameInstance()->GetWorld())
	{
		World->GetTimerManager().ClearTimer(MusicFadeInHandle);
	}

	// Fade out the current track
	if (MusicComponent && MusicComponent->IsPlaying())
	{
		MusicComponent->FadeOut(FadeOutTime, 0.0f);
	}

	// Delay the fade-in so the two tracks don't overlap
	if (FadeOutTime > 0.0f)
	{
		UWorld* World = GetGameInstance()->GetWorld();
		if (World)
		{
			USoundBase* TrackToPlay = Track;
			float InTime             = FadeInTime;

			World->GetTimerManager().SetTimer(
				MusicFadeInHandle,
				[this, TrackToPlay, InTime]()
				{
					PlayMusicTrack(TrackToPlay, InTime);
				},
				FadeOutTime,
				false
			);
		}
	}
	else
	{
		PlayMusicTrack(Track, FadeInTime);
	}
}

void UAudioSubsystem::StopMusic(float FadeOutTime)
{
	if (!MusicComponent || !MusicComponent->IsPlaying())
	{
		return;
	}

	// Cancel any pending fade-in
	if (UWorld* World = GetGameInstance()->GetWorld())
	{
		World->GetTimerManager().ClearTimer(MusicFadeInHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("[Audio] StopMusic (FadeOut=%.1fs)."), FadeOutTime);

	if (FadeOutTime > 0.0f)
	{
		MusicComponent->FadeOut(FadeOutTime, 0.0f);
	}
	else
	{
		MusicComponent->Stop();
	}

	CurrentMusicState = EMusicState::None;
	CurrentMusicTrack = nullptr;
}

// ---------------------------------------------------------------------------
// SFX API — world-positioned
// ---------------------------------------------------------------------------

void UAudioSubsystem::PlaySFXAtLocation(USoundBase* Sound, FVector Location,
	float VolumeMultiplier, float PitchMultiplier)
{
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Audio] PlaySFXAtLocation: null sound."));
		return;
	}

	UWorld* World = GetGameInstance()->GetWorld();
	if (!World) { return; }

	UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, VolumeMultiplier, PitchMultiplier);
}

UAudioComponent* UAudioSubsystem::PlaySFXAttached(USoundBase* Sound, USceneComponent* AttachTo,
	float VolumeMultiplier, float PitchMultiplier)
{
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Audio] PlaySFXAttached: null sound."));
		return nullptr;
	}

	if (!AttachTo)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Audio] PlaySFXAttached: null component — falling back to world origin."));
		PlaySFXAtLocation(Sound, FVector::ZeroVector, VolumeMultiplier, PitchMultiplier);
		return nullptr;
	}

	return UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, NAME_None, FVector(ForceInit),
		EAttachLocation::KeepRelativeOffset, /*bStopWhenAttachedToDestroyed=*/false,
		VolumeMultiplier, PitchMultiplier);
}

// ---------------------------------------------------------------------------
// SFX API — 2D
// ---------------------------------------------------------------------------

void UAudioSubsystem::PlayUISound(USoundBase* Sound)
{
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Audio] PlayUISound: null sound."));
		return;
	}

	UWorld* World = GetGameInstance()->GetWorld();
	if (!World) { return; }

	UGameplayStatics::PlaySound2D(World, Sound);
}

void UAudioSubsystem::PlayUISoundEvent(EUISoundEvent Event)
{
	const TObjectPtr<USoundBase>* Found = UISoundMap.Find(Event);
	if (!Found || !(*Found))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Audio] PlayUISoundEvent: no sound mapped for event %d."), (int32)Event);
		return;
	}

	PlayUISound(*Found);

	UE_LOG(LogTemp, Verbose, TEXT("[Audio] UI sound event %d -> '%s'."), (int32)Event, *GetNameSafe(*Found));
}

// ---------------------------------------------------------------------------
// Volume control
// ---------------------------------------------------------------------------

void UAudioSubsystem::SetSoundClassVolume(USoundClass* SoundClass, float Volume, float FadeTime)
{
	if (!SoundClass || !MasterSoundMix)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Audio] SetSoundClassVolume: missing SoundClass or MasterSoundMix."));
		return;
	}

	UWorld* World = GetGameInstance()->GetWorld();
	if (!World) { return; }

	UGameplayStatics::SetSoundMixClassOverride(World, MasterSoundMix, SoundClass, Volume, 1.0f, FadeTime);

	UE_LOG(LogTemp, Log, TEXT("[Audio] Volume: '%s' -> %.2f (Fade=%.1fs)"),
		*GetNameSafe(SoundClass), Volume, FadeTime);
}

// ---------------------------------------------------------------------------
// Volume control — category API
// ---------------------------------------------------------------------------

float UAudioSubsystem::GetCategoryVolume(ENevergoneAudioCategory Category) const
{
	// The persisted player choice in UNevergoneGameUserSettings is the single
	// source of truth — the SoundMix only mirrors that value at runtime.
	if (const UNevergoneGameUserSettings* Settings = UNevergoneGameUserSettings::GetNevergoneGameUserSettings())
	{
		return Settings->GetCategoryVolume(Category);
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Audio] GetCategoryVolume: NevergoneGameUserSettings unavailable — returning 1.0"));
	return 1.0f;
}

void UAudioSubsystem::SetCategoryVolume(ENevergoneAudioCategory Category, float NewVolume)
{
	const float ClampedVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);

	// 1. Persist the choice. The settings object writes to disk synchronously.
	if (UNevergoneGameUserSettings* Settings = UNevergoneGameUserSettings::GetNevergoneGameUserSettings())
	{
		Settings->SetCategoryVolume(Category, ClampedVolume);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Audio] SetCategoryVolume: NevergoneGameUserSettings unavailable — choice NOT persisted ")
			TEXT("(check DefaultEngine.ini GameUserSettingsClassName entry)"));
	}

	// 2. Apply to the live SoundMix.
	const TObjectPtr<USoundClass>* SoundClassPtr = CategorySoundClasses.Find(Category);
	if (!SoundClassPtr || !*SoundClassPtr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Audio] SetCategoryVolume: no USoundClass mapped for category %d — assign one in BP_MyGameInstance.CategorySoundClasses"),
			static_cast<int32>(Category));
		return;
	}

	// FadeTime 0 — Settings sliders need immediate, snappy feedback.
	SetSoundClassVolume(*SoundClassPtr, ClampedVolume, 0.0f);
}

void UAudioSubsystem::ApplyAllVolumesFromSettings()
{
	// SoundMix overrides only take effect while the mix is "pushed" onto the
	// audio device. UGameplayStatics::SetSoundMixClassOverride registers the
	// new volume value, but if the mix isn't active the override is dormant.
	//
	// Push must happen AFTER the audio device exists. The earliest reliable
	// point in the editor (PIE) is PostLoadMapWithWorld — the GameInstance
	// path runs too early, before the world's audio device is created. We
	// gate via bMasterMixPushed so this runs at most once per session;
	// pushing the same mix multiple times stacks reference counts and Pop on
	// Deinitialize would only decrement once.
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!bMasterMixPushed && World && MasterSoundMix)
	{
		UGameplayStatics::PushSoundMixModifier(World, MasterSoundMix);
		bMasterMixPushed = true;

		UE_LOG(LogTemp, Log,
			TEXT("[Audio] ApplyAllVolumesFromSettings: pushed MasterSoundMix '%s' onto audio device"),
			*GetNameSafe(MasterSoundMix));
	}

	// Cheap helper: re-applies every category volume from persisted settings.
	// Safe to call multiple times (idempotent).
	const ENevergoneAudioCategory AllCategories[] = {
		ENevergoneAudioCategory::Master,
		ENevergoneAudioCategory::Music,
		ENevergoneAudioCategory::SFX,
		ENevergoneAudioCategory::UI,
	};

	for (ENevergoneAudioCategory Category : AllCategories)
	{
		const float Volume = GetCategoryVolume(Category);

		// Apply directly via SetSoundClassVolume to avoid the redundant SaveSettings
		// disk write that SetCategoryVolume would trigger when value is unchanged.
		const TObjectPtr<USoundClass>* SoundClassPtr = CategorySoundClasses.Find(Category);
		if (SoundClassPtr && *SoundClassPtr)
		{
			SetSoundClassVolume(*SoundClassPtr, Volume, 0.0f);
		}
		else
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("[Audio] ApplyAllVolumesFromSettings: skipping category %d — no USoundClass mapped"),
				static_cast<int32>(Category));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Audio] ApplyAllVolumesFromSettings: live mix synced with player preferences"));
}

// ---------------------------------------------------------------------------
// PostLoadMapWithWorld handler — pushes the mix and re-applies volumes once
// the world's audio device is fully initialized. This is the fix for the
// "initial 0% slider has no audible effect" bug: GameInstance::Init's first
// apply runs too early, before the audio device exists, so the override is
// dropped silently.
// ---------------------------------------------------------------------------

void UAudioSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	UE_LOG(LogTemp, Log,
		TEXT("[Audio] HandlePostLoadMap: world='%s' — re-applying volumes now that audio device is up"),
		*GetNameSafe(LoadedWorld));

	// ApplyAllVolumesFromSettings is idempotent and handles the lazy push
	// (it only pushes if !bMasterMixPushed and the mix is valid). Calling
	// it again here guarantees the very first audible state matches the
	// player's persisted preferences, with the audio device fully ready.
	ApplyAllVolumesFromSettings();
}

// ---------------------------------------------------------------------------
// Combat integration
// ---------------------------------------------------------------------------

void UAudioSubsystem::BindToCombatEventBus(UCombatEventBus* EventBus)
{
	if (!EventBus)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Audio] BindToCombatEventBus: null EventBus."));
		return;
	}

	// Remove previous bindings to avoid duplicates across battles
	if (UCombatEventBus* OldBus = BoundEventBus.Get())
	{
		OldBus->OnDamageApplied.RemoveAll(this);
		OldBus->OnUnitDied.RemoveAll(this);
		OldBus->OnHealApplied.RemoveAll(this);
	}

	EventBus->OnDamageApplied.AddUObject(this, &UAudioSubsystem::HandleDamageApplied);
	EventBus->OnUnitDied.AddUObject(this,      &UAudioSubsystem::HandleUnitDied);
	EventBus->OnHealApplied.AddUObject(this,   &UAudioSubsystem::HandleHealApplied);

	BoundEventBus = EventBus;

	UE_LOG(LogTemp, Log, TEXT("[Audio] Bound to CombatEventBus."));
}

// ---------------------------------------------------------------------------
// Context change handler
// ---------------------------------------------------------------------------

void UAudioSubsystem::HandleGameContextChanged(EGameContextState NewState)
{
	// Music transitions are now driven by TowerFloorGameMode directly.
	// This handler only covers the level-transition case (fade out).
	if (NewState == EGameContextState::Transition)
	{
		StopMusic(1.0f);
		UE_LOG(LogTemp, Log, TEXT("[Audio] Level transition detected — music faded out."));
	}
}

// ---------------------------------------------------------------------------
// CombatEventBus handlers — fallback SFX for events without a specific sound
// ---------------------------------------------------------------------------

void UAudioSubsystem::HandleDamageApplied(ACharacterBase* Source, ACharacterBase* Target, int32 Amount)
{
	if (!Target || !DefaultHitSFX) { return; }

	// Only fires if the ability's PlayImpactSFX did NOT play a specific sound.
	// If the ability has an ImpactSound assigned, it already called PlaySFXAtLocation
	// directly — this is purely the fallback for abilities without one.
	PlaySFXAtLocation(DefaultHitSFX, Target->GetActorLocation());
}

void UAudioSubsystem::HandleUnitDied(ACharacterBase* Unit)
{
	if (!Unit || !DefaultDeathSFX) { return; }

	PlaySFXAtLocation(DefaultDeathSFX, Unit->GetActorLocation());

	UE_LOG(LogTemp, Log, TEXT("[Audio] Death SFX — Unit: %s"), *GetNameSafe(Unit));
}

void UAudioSubsystem::HandleHealApplied(ACharacterBase* Source, ACharacterBase* Target, int32 Amount)
{
	if (!Target || !DefaultHealSFX) { return; }

	PlaySFXAtLocation(DefaultHealSFX, Target->GetActorLocation());
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void UAudioSubsystem::PlayMusicTrack(USoundBase* Track, float FadeInTime)
{
	if (!Track) { return; }

	UWorld* World = GetGameInstance()->GetWorld();
	if (!World) { return; }

	if (!MusicComponent)
	{
		// Create the component once; it persists for the entire game session
		MusicComponent = UGameplayStatics::CreateSound2D(
			World,
			Track,
			1.0f,    // Volume
			1.0f,    // Pitch
			0.0f,    // StartTime
			nullptr, // Concurrency
			true,    // bPersistAcrossLevelTransitions
			false    // bAutoDestroy
		);

		if (!MusicComponent)
		{
			UE_LOG(LogTemp, Error, TEXT("[Audio] Failed to create music AudioComponent."));
			return;
		}
	}
	else
	{
		MusicComponent->SetSound(Track);
	}

	MusicComponent->FadeIn(FadeInTime, 1.0f);

	UE_LOG(LogTemp, Log, TEXT("[Audio] Playing: '%s' (FadeIn=%.1fs)"), *GetNameSafe(Track), FadeInTime);
}