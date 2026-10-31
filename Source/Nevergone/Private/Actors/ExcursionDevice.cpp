// Copyright Xyzto Works


#include "Actors/ExcursionDevice.h"

#include "Nevergone.h"
#include "Actors/ExcursionNoiseMaskVolume.h"
#include "ActorComponents/InteractableComponent.h"
#include "Audio/AudioSubsystem.h"
#include "Data/NoiseDefinition.h"
#include "World/ExcursionNoiseSubsystem.h"

#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Sound/SoundBase.h"

AExcursionDevice::AExcursionDevice()
{
	PrimaryActorTick.bCanEverTick = false;

	// Takes the interact trace: UInteractableComponent makes every primitive
	// with collision block the Interactable channel.
	DeviceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeviceMesh"));
	SetRootComponent(DeviceMesh);

	LoopAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("LoopAudio"));
	LoopAudio->SetupAttachment(DeviceMesh);
	LoopAudio->bAutoActivate = false;

	CreateDefaultSubobject<UInteractableComponent>(TEXT("InteractableComponent"));
}

void AExcursionDevice::BeginPlay()
{
	Super::BeginPlay();

	if (!MaskVolume)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionDevice] %s has no MaskVolume; switching it on masks nothing"),
			*GetNameSafe(this));
	}
	else if (MaskVolume->GetDevice() && MaskVolume->GetDevice() != this)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionDevice] %s shares %s with %s; the last one to start switches it"),
			*GetNameSafe(this), *GetNameSafe(MaskVolume), *GetNameSafe(MaskVolume->GetDevice()));
	}

	if (TurnOnNoise && TurnOnNoise->NoiseTag != TAG_Noise_Device.GetTag())
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionDevice] %s turn-on noise %s is tagged %s, not Noise.Device; enemies will not investigate the device"),
			*GetNameSafe(this), *GetNameSafe(TurnOnNoise), *TurnOnNoise->NoiseTag.ToString());
	}

	if (MaskVolume)
	{
		MaskVolume->SetDevice(this);
	}

	if (LoopSound)
	{
		LoopAudio->SetSound(LoopSound);
	}

	// A loop with no attenuation of its own (neither on the component nor on
	// the sound) takes the project's world default, like every world SFX.
	const UGameInstance* GameInstance = GetGameInstance();

	const UAudioSubsystem* Audio = GameInstance ? GameInstance->GetSubsystem<UAudioSubsystem>() : nullptr;

	if (Audio && !LoopAudio->AttenuationSettings && !LoopAudio->bOverrideAttenuation)
	{
		LoopAudio->SetAttenuationSettings(Audio->ResolveWorldAttenuation(LoopAudio->Sound));
	}

	// The placed state, silently: nobody switched it on.
	bIsOn = bStartOn;

	ApplyState();

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionDevice] %s starts %s (mask %s)"),
		*GetNameSafe(this), bIsOn ? TEXT("on") : TEXT("off"), *GetNameSafe(MaskVolume));
}

void AExcursionDevice::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LoopAudio->Stop();

	// A volume left without its device goes back to always active.
	if (MaskVolume && MaskVolume->GetDevice() == this)
	{
		MaskVolume->SetDevice(nullptr);
		MaskVolume->SetMaskActive(true);
	}

	Super::EndPlay(EndPlayReason);
}

// ===========================================================================
// IInteractable
// ===========================================================================

void AExcursionDevice::Interact_Implementation(AActor* Interactor)
{
	Toggle(Interactor);
}

// ===========================================================================
// Switching
// ===========================================================================

void AExcursionDevice::SetOn(bool bNewOn, AActor* Switcher)
{
	if (bIsOn == bNewOn)
	{
		return;
	}

	bIsOn = bNewOn;

	// The noise goes out before the mask goes on. The volume never masks
	// the device's own noise anyway: hearing processes it a frame or two
	// later, when the mask already holds.
	if (bIsOn)
	{
		EmitTurnOnNoise(Switcher);
	}

	ApplyState();

	if (bIsOn)
	{
		BP_OnSwitchedOn();
	}
	else
	{
		BP_OnSwitchedOff();
	}

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExcursionDevice] %s switched %s by %s (mask %s)"),
		*GetNameSafe(this), bIsOn ? TEXT("on") : TEXT("off"), *GetNameSafe(Switcher), *GetNameSafe(MaskVolume));
}

void AExcursionDevice::Toggle(AActor* Switcher)
{
	SetOn(!bIsOn, Switcher);
}

FVector AExcursionDevice::GetNoiseLocation() const
{
	return DeviceMesh->Bounds.Origin;
}

void AExcursionDevice::ApplyState()
{
	if (MaskVolume)
	{
		MaskVolume->SetMaskActive(bIsOn);
	}

	// LoopSound, or a sound set on the component in the Blueprint.
	if (!LoopAudio->Sound)
	{
		return;
	}

	if (bIsOn && !LoopAudio->IsPlaying())
	{
		LoopAudio->Play();
	}
	else if (!bIsOn)
	{
		LoopAudio->Stop();
	}
}

void AExcursionDevice::EmitTurnOnNoise(AActor* Switcher)
{
	if (!TurnOnNoise)
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExcursionDevice] %s has no turn-on noise; switched on silently"),
			*GetNameSafe(this));

		return;
	}

	UExcursionNoiseSubsystem* Noise = UExcursionNoiseSubsystem::Get(this);

	if (!Noise)
	{
		return;
	}

	// A place noise is heard whoever made it; with nobody to blame, the
	// device itself is the instigator, so enemies still hear it.
	AActor* NoiseInstigator = Switcher ? Switcher : this;

	// The device is the noise's source: it never counts as a wall for it,
	// and its own volume never masks it.
	Noise->EmitNoise(TurnOnNoise, GetNoiseLocation(), NoiseInstigator, nullptr, this);
}
