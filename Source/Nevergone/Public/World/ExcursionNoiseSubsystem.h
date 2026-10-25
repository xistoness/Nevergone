// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ExcursionNoiseSubsystem.generated.h"

class AExcursionNoiseMaskVolume;
class UNoiseDefinition;
class UPhysicalMaterial;

// ---------------------------------------------------------------------------
// UExcursionNoiseSubsystem
//
// The single entry point for gameplay noise. Every noise source (footsteps,
// doors, props, distraction items) calls EmitNoise; nothing else calls
// ReportNoiseEvent or plays a noise sound on its own.
//
// A noise is resolved in three layers:
//   1. Source   (UNoiseDefinition)            what happened
//   2. Surface  (UNevergonePhysicalMaterial)  where it happened, applied here
//   3. Path     (occlusion, masking)          applied per listener by
//                                             UExplorationPerceptionComponent
//
// The surface is applied at emission because it can amplify: the engine's
// own hearing check has to see the amplified loudness or it would discard a
// noise the surface made audible. Occlusion and masking only ever reduce, so
// filtering them after the engine check is exact.
//
// Also owns the registry of AExcursionNoiseMaskVolume, so listeners can ask
// how masked a point is without iterating actors.
//
// Debug: Nevergone.Noise.Debug 1 draws every emitted noise and every hearing
// decision (green heard, red filtered out).
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UExcursionNoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	static UExcursionNoiseSubsystem* Get(const UObject* WorldContextObject);

	// Plays the noise's sound for the player and reports it to AI hearing.
	//
	// Instigator is whoever caused the noise (the one walking, opening the
	// door, bumping the prop, throwing the item). Enemy hearing
	// accepts stimuli attributed to a survivor body: the player's survivor or
	// a recruited follower (AExcursionSurvivorNPC::IsSurvivorBody). Anything
	// else (an enemy opening a door) is heard by the player only. The
	// exception is a place noise (IsHeardAsPlaceNoise): heard whoever made
	// it, pointing enemies at a place rather than at a survivor.
	//
	// Surface is optional: when known it selects the sound variant and
	// scales the AI loudness.
	void EmitNoise(const UNoiseDefinition* Definition, const FVector& Location,
		AActor* Instigator, const UPhysicalMaterial* Surface = nullptr);

	// --- Place noise ---

	// True when an enemy hears this stimulus as a place noise: its tag is in
	// the place-noise list (today only Noise.Scream) and it was not made by
	// the listener itself. The instigator does not matter otherwise.
	static bool IsHeardAsPlaceNoise(FName StimulusTag, bool bMadeByListener);

	// --- Masking ---

	void RegisterMaskVolume(AExcursionNoiseMaskVolume* Volume);

	void UnregisterMaskVolume(AExcursionNoiseMaskVolume* Volume);

	// Hearing multiplier at a point: the lowest multiplier among the mask
	// volumes that contain it, or 1 when none does.
	float GetHearingMaskAt(const FVector& Location) const;

	// --- Debug ---

	static bool IsDebugEnabled();

protected:

	// Game and PIE only. Editor animation previews fire footstep notifies
	// too, and must not report AI noise.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:

	TArray<TWeakObjectPtr<AExcursionNoiseMaskVolume>> MaskVolumes;
};
