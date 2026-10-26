// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

// What stands between an enemy and the survivor it is about to catch.
enum class ECaptureLineBlock : uint8
{
	// A free line: the capture may happen.
	None,

	// A closed, latched or locked door across the doorway between them
	// (ADoor::FindClosedDoorBetween). Decided by the door's state, not by
	// what the leaf covers.
	ClosedDoor,

	// Fixed scenery across the line: walls, door frames, fixed furniture.
	Scenery,
};

// ---------------------------------------------------------------------------
// CaptureRules
//
// The capture line rules, with no world access so they can be tested on
// their own. A capture only happens with a free line between the enemy and
// its victim: neither a closed door nor fixed scenery between them. The door
// leaf, open or closed, never counts as scenery (a closed door is decided by
// its state), and neither do knockable props nor characters, alive or not.
// AExplorationEnemyController::FindCaptureLineBlock gathers both from the
// world and decides through here.
// ---------------------------------------------------------------------------
namespace CaptureRules
{
	// A closed door between them wins over scenery: it is the door the enemy
	// has to go through, whatever wall the line also clips.
	NEVERGONE_API ECaptureLineBlock ResolveLineBlock(bool bClosedDoorBetween, bool bSceneryBetween);

	// The catch fires only within catch distance and with a free line.
	NEVERGONE_API bool CanCatch(bool bWithinCatchDistance, ECaptureLineBlock Block);

	// True when a blocking hit on the capture line trace is fixed scenery.
	//   ObjectType   the object type of the component hit
	//   bPartOfDoor  the component belongs to a door (its box or its leaf)
	// Scenery is WorldStatic or WorldDynamic. Characters (Pawn, and
	// PhysicsBody once a ragdoll) and knockable props (PhysicsBody) are not.
	NEVERGONE_API bool IsSceneryHit(ECollisionChannel ObjectType, bool bPartOfDoor);
}
