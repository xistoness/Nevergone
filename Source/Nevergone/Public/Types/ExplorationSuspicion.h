// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"

// What one tick of an open suspicion window amounted to. Returned by
// FExplorationSuspicion::Advance so the controller can act and log on the
// edges without tracking them itself.
enum class EExplorationSuspicionStep : uint8
{
	// The target is in view and the confirmation grows.
	Watching,

	// The target came back into view inside the tolerance. The confirmation
	// resumes from where it stopped.
	Regained,

	// The target just went out of view. The tolerance starts.
	LostSight,

	// Still out of view, still inside the tolerance.
	Tolerating,

	// Out of view for the whole tolerance: the window ends unconfirmed.
	Expired,
};

// ---------------------------------------------------------------------------
// FExplorationSuspicion
//
// The sight confirmation of one enemy: how long its target has been in view,
// and, while the enemy is Suspicious, how long the target has been out of
// view and which noise was heard meanwhile. Plain struct with no world
// access, so the rules can be tested on their own. Owned and fed by
// AExplorationEnemyController.
//
// Rules of an open window:
//   - Losing sight of the target for less than the tolerance does not end
//     it. Meanwhile the confirmation neither grows nor resets; seeing the
//     target again resumes it.
//   - A noise heard during the window does not end it either. The most
//     recent one is held, and when the window expires the enemy investigates
//     the most recent stimulus: the held noise if it was heard after the
//     any survivor last came into view, the last seen point otherwise. A
//     survivor coming into view (the target again after a loss, or another
//     one) therefore drops the held noise.
//   - The confirmation belongs to the enemy. Losing the target and seeing
//     another survivor inside the tolerance switches to the new one without
//     resetting it; the enemy then chases whoever is in view when it
//     completes.
// ---------------------------------------------------------------------------
struct FExplorationSuspicion
{
	// Clears everything: no contact, no window, no held noise.
	void Reset();

	// Opens a window on a target seen at TargetLocation. Keeps the contact
	// accumulated by the tick that opened it.
	void Open(const FVector& TargetLocation);

	// Adds contact time outside an open window.
	void AddContact(float DeltaTime);

	// Restarts the confirmation on a newly acquired target outside an open
	// window. Inside a window the target changes through SwitchTarget.
	void RestartContact();

	// Inside an open window, the target went out of view and another survivor
	// is in view. The confirmation is the enemy's, not a survivor's: it keeps
	// its progress and the gap ends. The new survivor is the most recent
	// sighting, which supersedes a held noise and becomes the last seen point.
	void SwitchTarget(const FVector& NewTargetLocation);

	// One tick of an open window. TargetLocation is read only when the
	// target is in view.
	EExplorationSuspicionStep Advance(bool bTargetInView, const FVector& TargetLocation,
		float DeltaTime, float ToleranceSeconds);

	// Holds a noise heard during the window, replacing an older one.
	void HoldNoise(const FVector& NoiseLocation);

	// Drops the held noise. A confirmed window has no use for it.
	void DiscardHeldNoise();

	// Where an expired window sends the enemy: the held noise, or else the
	// last seen point.
	FVector ResolveStimulus() const;

	float GetContactSeconds() const { return ContactSeconds; }

	float GetSightLossSeconds() const { return SightLossSeconds; }

	bool IsSightLost() const { return bSightLost; }

	bool HasHeldNoise() const { return bHasHeldNoise; }

	const FVector& GetHeldNoiseLocation() const { return HeldNoiseLocation; }

	const FVector& GetLastSeenLocation() const { return LastSeenLocation; }

private:

	// Continuous contact with the target; gaps inside the tolerance are
	// neither counted nor a reset.
	float ContactSeconds = 0.0f;

	// Length of the current gap, while the target is out of view.
	float SightLossSeconds = 0.0f;

	bool bSightLost = false;

	// Where the target was on the last tick it was in view.
	FVector LastSeenLocation = FVector::ZeroVector;

	bool bHasHeldNoise = false;

	FVector HeldNoiseLocation = FVector::ZeroVector;
};
