// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Chaos/ChaosEngineInterface.h"
#include "NoiseDefinition.generated.h"

class USoundBase;

// ---------------------------------------------------------------------------
// UNoiseDefinition
//
// What happened: one kind of gameplay noise (a running footstep, a door
// opening, a prop hitting the floor). It carries both halves of a noise:
//   - AI: how loud it is to enemies (Loudness, MaxRange, NoiseTag).
//   - Audio: what the player hears, optionally varied by surface.
//
// It does not know where the noise happened or who hears it. The surface is
// supplied by the emitter (UNevergonePhysicalMaterial multiplies Loudness),
// and occlusion and masking are resolved per listener.
//
// Emitted only through UExcursionNoiseSubsystem::EmitNoise. Distance
// attenuation, occlusion filtering and reverb of the audio live on the sound
// assets themselves, not here.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UNoiseDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	// Category of the noise. Forwarded to AI as the stimulus tag so enemy
	// types can react differently to it later.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise", meta = (Categories = "Noise"))
	FGameplayTag NoiseTag;

	// --- AI ---

	// Scales the listener's hearing radius: 1 is heard across the whole
	// radius, 0.5 across half of it. 0 makes the noise audio-only, which is
	// what enemy footsteps use.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise|AI", meta = (ClampMin = "0.0"))
	float Loudness = 1.0f;

	// Hard cap in centimetres, whatever the loudness and the listener.
	// 0 means no cap.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise|AI", meta = (ClampMin = "0.0"))
	float MaxRange = 0.0f;

	// --- Audio ---

	// Played when the surface has no entry in SurfaceSounds, or when the
	// emitter does not know the surface (doors).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise|Audio")
	TObjectPtr<USoundBase> DefaultSound;

	// Per-surface variants. A footstep on metal and a prop falling on metal
	// are different sounds, so the variants live on the noise, not on the
	// surface.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise|Audio")
	TMap<TEnumAsByte<EPhysicalSurface>, TObjectPtr<USoundBase>> SurfaceSounds;

	// How much the pitch may drift each time this noise plays, as a fraction:
	// 0.1 picks a random pitch between 0.9 and 1.1. This is what keeps a
	// repeated sound, above all footsteps, from sounding like a loop. 0
	// plays the asset untouched.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise|Audio", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PitchVariation = 0.0f;

	// Same idea for volume. Smaller values than the pitch usually sound
	// right: a step that changes loudness a lot reads as a different step.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Noise|Audio", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float VolumeVariation = 0.0f;

	// Random multipliers for one playback. Both are 1 when variation is 0.
	// Affects only what the player hears: the AI loudness is deterministic,
	// so hearing never depends on a dice roll.
	void RollAudioVariation(float& OutVolumeMultiplier, float& OutPitchMultiplier) const;

	// Sound for the given surface, falling back to DefaultSound. May be null.
	USoundBase* ResolveSound(EPhysicalSurface Surface) const;

	bool ReportsToAI() const { return Loudness > 0.0f; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
