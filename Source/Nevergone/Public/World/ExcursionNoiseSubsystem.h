// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ExcursionNoiseSubsystem.generated.h"

class AExcursionNoiseMaskVolume;
class UNoiseDefinition;
class UPhysicalMaterial;

// ---------------------------------------------------------------------------
// FExcursionNoiseSourceLog
//
// The objects that made the noises of the last moment (the door that was
// banged, the device that rang). The engine's hearing stimulus carries only
// the instigator, so a listener looks the source up here by instigator,
// location and tag, and leaves it out of the occlusion trace: the door does
// not muffle its own noise. Plain struct so it can be tested on its own.
// ---------------------------------------------------------------------------
struct NEVERGONE_API FExcursionNoiseSourceLog
{
	// How long an entry is kept, in world seconds: hearing processes a
	// reported noise within a frame or two.
	static constexpr double LifetimeSeconds = 1.0;

	void Add(const UObject* Instigator, const FVector& Location, FName Tag, const UObject* Source, double Now);

	// The source of the most recent matching noise still in the log, or null.
	const UObject* Find(const UObject* Instigator, const FVector& Location, FName Tag, double Now) const;

private:

	struct FEntry
	{
		TWeakObjectPtr<const UObject> Instigator;
		TWeakObjectPtr<const UObject> Source;
		FVector Location = FVector::ZeroVector;
		FName Tag;
		double Time = 0.0;
	};

	TArray<FEntry> Entries;
};

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
	//
	// Source is optional: the world object the noise comes out of (a door),
	// when it is not the instigator. Hearing occlusion ignores it, so an
	// object never counts as a wall for its own noise.
	void EmitNoise(const UNoiseDefinition* Definition, const FVector& Location,
		AActor* Instigator, const UPhysicalMaterial* Surface = nullptr, const AActor* Source = nullptr);

	// The Source a noise was emitted with, for a listener that heard it
	// (instigator, stimulus location and tag). Null when it had none.
	const AActor* FindNoiseSource(const AActor* Instigator, const FVector& Location, FName Tag) const;

	// --- Place noise ---

	// True when an enemy hears this stimulus as a place noise: its tag is in
	// the place-noise list (Noise.Scream, Noise.DoorKnock, Noise.DoorBreak,
	// Noise.Device)
	// and it was not made by the listener itself. The instigator does not
	// matter otherwise.
	static bool IsHeardAsPlaceNoise(FName StimulusTag, bool bMadeByListener);

	// --- Masking ---

	void RegisterMaskVolume(AExcursionNoiseMaskVolume* Volume);

	void UnregisterMaskVolume(AExcursionNoiseMaskVolume* Volume);

	// Hearing multiplier at a point: the lowest multiplier among the mask
	// volumes that contain it and mask a noise from NoiseSource
	// (AExcursionNoiseMaskVolume::MasksNoiseFrom), or 1 when none does.
	// NoiseSource is the Source the noise was emitted with, null when none.
	float GetHearingMaskAt(const FVector& Location, const AActor* NoiseSource) const;

	// --- Debug ---

	static bool IsDebugEnabled();

protected:

	// Game and PIE only. Editor animation previews fire footstep notifies
	// too, and must not report AI noise.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:

	TArray<TWeakObjectPtr<AExcursionNoiseMaskVolume>> MaskVolumes;

	FExcursionNoiseSourceLog SourceLog;
};
