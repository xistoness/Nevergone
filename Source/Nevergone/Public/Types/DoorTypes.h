// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "DoorTypes.generated.h"

// ---------------------------------------------------------------------------
// EDoorState
//
// Runtime state of an ADoor. Only Locked / not-Locked is persisted (the
// door's bIsUnlocked flag). The rest is transient: a door restored from save
// always materializes Closed (or Locked), unlatched and whole, and an open
// door stays open until somebody closes it.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EDoorState : uint8
{
	Locked	UMETA(DisplayName = "Locked"),	// Blocks pawns; only the player with the key opens it.
	Closed	UMETA(DisplayName = "Closed"),	// Blocks pawns; anyone who reaches it can open it.
	Open	UMETA(DisplayName = "Open"),	// Walkable; stays open until somebody closes it.
	Latched	UMETA(DisplayName = "Latched"),	// Closed with the latch on: opens only from the latch side.
	Broken	UMETA(DisplayName = "Broken"),	// Broken down by an enemy: open for good, never closes again.
};

// Which side of the doorway the latch is on, per placed door. Front is the
// side the door's crossing axis points to: the collision box's X axis, which
// is the actor's forward (red) arrow unless the box was rotated.
UENUM(BlueprintType)
enum class EDoorLatchSide : uint8
{
	None	UMETA(DisplayName = "No latch"),
	Front	UMETA(DisplayName = "Front (+X)"),
	Back	UMETA(DisplayName = "Back (-X)"),
};

// What an AI agent (enemy or follower) does when its path reaches a door.
UENUM()
enum class EDoorAgentPassage : uint8
{
	// Walk through without stopping.
	Pass,

	// Stop, open the door after the door's agent delay, then walk through.
	OpenFirst,

	// Stop: the door does not open for an agent. An enemy knocks until it
	// breaks it; anyone else waits until somebody opens it.
	Barred,

	// The door is a wall: no path crosses it.
	Blocked,
};

// What the player's interact does to a door.
UENUM()
enum class EDoorInteraction : uint8
{
	None,

	// Locked key door: unlock with the key, or the locked feedback.
	UseKey,

	Open,

	Close,

	// Put the latch on a closed door.
	Latch,

	// Close the open door, then put the latch on.
	CloseAndLatch,

	// Take the latch off, leaving the door closed.
	Unlatch,

	// Take the latch off and open, in one go.
	UnlatchAndOpen,

	// The leaf moves, catches and comes back: the latch holds it.
	Jolt,
};

// ---------------------------------------------------------------------------
// FDoorKnock
//
// An enemy knocking at a barred door, counted in the enemy's own (dilated)
// time. Knocks on the first step and every interval after; breaks the door
// once the door's break time has been spent knocking.
// ---------------------------------------------------------------------------
struct NEVERGONE_API FDoorKnock
{
	struct FStep
	{
		// Play a knock now.
		bool bKnock = false;

		// The door gives way now.
		bool bBreak = false;
	};

	FStep Advance(float DeltaSeconds, float KnockIntervalSeconds, float BreakSeconds);

	void Reset();

	float GetElapsedSeconds() const { return ElapsedSeconds; }

	bool HasStarted() const { return bStarted; }

private:

	float ElapsedSeconds = 0.0f;

	// Elapsed time at which the next knock plays.
	float NextKnockAtSeconds = 0.0f;

	bool bStarted = false;
};

// ---------------------------------------------------------------------------
// DoorRules
//
// The door rules, with no world access so they can be tested on their own.
// Every decision an agent or the player makes at a door goes through here,
// so the follower passing the latch and holding the door extend one place.
// ---------------------------------------------------------------------------
namespace DoorRules
{
	NEVERGONE_API EDoorAgentPassage ResolveAgentPassage(EDoorState State);

	// Whether the door's nav link is offered to pathfinding at all. A link
	// that is off makes the door a wall for every path query.
	NEVERGONE_API bool IsAgentLinkEnabled(EDoorState State);

	// True when somebody at OpenerSide (signed distance along the doorway
	// axis) stands on the latch side. Never for a door without a latch, nor
	// in the doorway plane.
	NEVERGONE_API bool IsOnLatchSide(EDoorLatchSide LatchSide, float OpenerSide);

	// True when holding interact does something: the latch side of a door
	// that can still be latched or unlatched.
	NEVERGONE_API bool CanHoldInteract(EDoorState State, bool bOnLatchSide);

	// What the player's interact does. A tap opens and closes (a latched
	// door from the latch side unlatches and opens; from the other side it
	// only jolts). A hold toggles the latch from the latch side, closing an
	// open door first. A hold that cannot do anything acts as a tap.
	NEVERGONE_API EDoorInteraction ResolveInteraction(EDoorState State, bool bOnLatchSide, bool bHold);

	// +1 or -1: the yaw direction that swings the leaf to the side of the
	// doorway opposite the opener, so it never swings into them.
	//   OpenerSide       the opener's distance along Axis from the doorway (signed)
	//   LeafCentre       middle of the closed leaf relative to its hinge
	//   OpenAngleDegrees how far the leaf turns when open
	//   Axis             the doorway's crossing axis (flat, unit)
	// +1 when the opener stands in the doorway plane.
	NEVERGONE_API float ChooseSwingSign(float OpenerSide, const FVector& LeafCentre, float OpenAngleDegrees, const FVector& Axis);

	// True when the door stands between an enemy and its victim as a wall
	// for the capture: closed, latched or locked. An open or broken door
	// never blocks it, whatever the leaf hides.
	NEVERGONE_API bool DoesDoorBlockCapture(EDoorState State);

	// True when the segment from LocalFrom to LocalTo goes through the
	// doorway: it crosses the door plane (X = 0) from one side to the other,
	// within the box's half width (Y) and half height (Z). Both points in the
	// collision box's frame, unscaled; BoxExtent is its scaled half size.
	// A point on the plane itself is on neither side.
	NEVERGONE_API bool DoesSegmentCrossDoorway(const FVector& LocalFrom, const FVector& LocalTo, const FVector& BoxExtent);

	// How far the leaf is off its rest pose ElapsedSeconds into a jolt:
	// straight out to AngleDegrees at half the duration, straight back, then
	// zero. Zero for a non-positive duration.
	NEVERGONE_API float GetJoltOffset(float ElapsedSeconds, float DurationSeconds, float AngleDegrees);
}
