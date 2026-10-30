// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "ExcursionDevice.generated.h"

class AExcursionNoiseMaskVolume;
class UAudioComponent;
class UNoiseDefinition;
class USoundBase;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// AExcursionDevice
//
// A machine in the level that can be switched on and off: a generator, a
// radio, a TV. Switching it on makes one noise and then covers the noises
// of its area; switching it off is silent.
//
// The area is an AExcursionNoiseMaskVolume placed in the level and drawn in
// the editor, referenced by MaskVolume. The device switches it: the volume
// masks only while the device is on, and never masks the device's own
// turn-on noise. A volume no device references is always active.
//
// Turning on, in order:
//   1. TurnOnNoise, a place noise (Noise.Device) emitted with the device as
//      its source: enemies within reach investigate the device, the device
//      never counts as a wall for it, and its own volume never masks it.
//   2. The mask goes on.
//   3. LoopSound starts, for the player only: it is no AI noise. A running
//      device makes no noise of its own after the first one.
//
// The player switches it through Interact. SetOn / Toggle are public and
// separate from Interact, for anybody else who works the device.
//
// The starting state is per placed instance (bStartOn). Deliberately not
// saveable: the excursion never saves halfway, so every load starts with
// every device as it was placed.
//
// Component layout:
//   DeviceMesh  (UStaticMeshComponent — root; mesh with simple collision,
//                assigned in the Blueprint subclass; takes the interact trace)
//   LoopAudio   (UAudioComponent      — the running sound, never auto-plays)
// ---------------------------------------------------------------------------
UCLASS(Blueprintable)
class NEVERGONE_API AExcursionDevice : public AActor,
                                       public IInteractable
{
	GENERATED_BODY()

public:

	AExcursionDevice();

	// -----------------------------------------------------------------------
	// IInteractable
	// -----------------------------------------------------------------------
	virtual void Interact_Implementation(AActor* Interactor) override;

	// -----------------------------------------------------------------------
	// Switching — shared by the player (through Interact) and anybody else
	// -----------------------------------------------------------------------

	// Switches the device on or off. Turning on makes the turn-on noise,
	// attributed to Switcher (the device itself when null). Does nothing
	// when the device already is in that state.
	void SetOn(bool bNewOn, AActor* Switcher);

	// Switches the device to the other state.
	void Toggle(AActor* Switcher);

	UFUNCTION(BlueprintPure, Category = "Device")
	bool IsOn() const { return bIsOn; }

	// Where the turn-on noise sounds: the centre of the mesh.
	FVector GetNoiseLocation() const;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// -----------------------------------------------------------------------
	// Configuration — designer-tweakable per placement
	// -----------------------------------------------------------------------

	// Whether the device is running when the level starts. Starting on makes
	// no noise.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Device|Config")
	bool bStartOn = false;

	// The area this device covers while on. Placed in the level and
	// referenced per instance; one device per volume.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Device|Config")
	TObjectPtr<AExcursionNoiseMaskVolume> MaskVolume;

	// -----------------------------------------------------------------------
	// Sound
	// -----------------------------------------------------------------------

	// Made once, on switching on. Must carry the Noise.Device tag: it is a
	// place noise, heard whoever switched the device on. Null is silent to
	// both the player and enemies.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Device|Sound")
	TObjectPtr<UNoiseDefinition> TurnOnNoise;

	// Plays while the device runs, for the player only. Must be a looping
	// sound.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Device|Sound")
	TObjectPtr<USoundBase> LoopSound;

	// -----------------------------------------------------------------------
	// Components
	// -----------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DeviceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> LoopAudio;

	// -----------------------------------------------------------------------
	// Blueprint hooks — implement in BP subclass to drive visuals
	// -----------------------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Device|Visuals")
	void BP_OnSwitchedOn();

	UFUNCTION(BlueprintImplementableEvent, Category = "Device|Visuals")
	void BP_OnSwitchedOff();

private:

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Device|State", meta = (AllowPrivateAccess = "true"))
	bool bIsOn = false;

	// Applies bIsOn to the mask and the loop. Makes no noise.
	void ApplyState();

	void EmitTurnOnNoise(AActor* Switcher);
};
