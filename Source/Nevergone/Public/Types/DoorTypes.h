// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "DoorTypes.generated.h"

// ---------------------------------------------------------------------------
// EDoorState
//
// Runtime state of an ADoor. Only Locked / not-Locked is persisted (the
// door's bIsUnlocked flag). Closed / Open is transient: a door restored from
// save always materializes Closed (or Locked), and an open door stays open
// until somebody closes it.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EDoorState : uint8
{
	Locked	UMETA(DisplayName = "Locked"),	// Blocks pawns; only the player with the key opens it.
	Closed	UMETA(DisplayName = "Closed"),	// Blocks pawns; anyone who reaches it can open it.
	Open	UMETA(DisplayName = "Open"),	// Walkable; stays open until somebody closes it.
};

// What an AI agent (enemy or follower) does when its path reaches a door.
UENUM()
enum class EDoorAgentPassage : uint8
{
	// Walk through without stopping.
	Pass,

	// Stop, open the door after the door's agent delay, then walk through.
	OpenFirst,

	// The door is a wall: no path crosses it.
	Blocked,
};

// ---------------------------------------------------------------------------
// DoorRules
//
// The door rules for AI, with no world access so they can be tested on their
// own. Every decision an agent makes at a door goes through here, so latch,
// knocking and a follower holding the door extend one place.
// ---------------------------------------------------------------------------
namespace DoorRules
{
	NEVERGONE_API EDoorAgentPassage ResolveAgentPassage(EDoorState State);

	// Whether the door's nav link is offered to pathfinding at all. A link
	// that is off makes the door a wall for every path query.
	NEVERGONE_API bool IsAgentLinkEnabled(EDoorState State);

	// +1 or -1: the yaw direction that swings the leaf to the side of the
	// doorway opposite the opener, so it never swings into them.
	//   OpenerSide       the opener's distance along Axis from the doorway (signed)
	//   LeafCentre       middle of the closed leaf relative to its hinge
	//   OpenAngleDegrees how far the leaf turns when open
	//   Axis             the doorway's crossing axis (flat, unit)
	// +1 when the opener stands in the doorway plane.
	NEVERGONE_API float ChooseSwingSign(float OpenerSide, const FVector& LeafCentre, float OpenAngleDegrees, const FVector& Axis);
}
