// Copyright Xyzto Works

#include "Audio/AudioSubsystem.h"

#include "GameInstance/GameContextManager.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "Characters/CharacterBase.h"

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
}

void UAudioSubsystem::Deinitialize()
{
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

void UAudioSubsystem::PlaySFXAtLocation(USoundBase* Sound, FVector Location)
{
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Audio] PlaySFXAtLocation: null sound."));
		return;
	}

	UWorld* World = GetGameInstance()->GetWorld();
	if (!World) { return; }

	UGameplayStatics::PlaySoundAtLocation(World, Sound, Location);
}

void UAudioSubsystem::PlaySFXAttached(USoundBase* Sound, USceneComponent* AttachTo)
{
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Audio] PlaySFXAttached: null sound."));
		return;
	}

	if (!AttachTo)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Audio] PlaySFXAttached: null component — falling back to world origin."));
		PlaySFXAtLocation(Sound, FVector::ZeroVector);
		return;
	}

	UGameplayStatics::SpawnSoundAttached(Sound, AttachTo);
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