// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"

class ADoor;

namespace NevergoneAI
{
	// True while the agent is walking a nav link (crossing a door). The
	// doorway is carved out of the navmesh, so a move planned or stopped
	// from inside it starts on the side the agent came from and pulls it
	// back: repaths and stops wait until this is false. An agent paused at a
	// closed door is still at the link start, on the navmesh, and is not
	// walking it.
	inline bool IsWalkingNavLink(const AAIController& Controller)
	{
		return Controller.ShouldPostponePathUpdates() &&
			Controller.GetMoveStatus() == EPathFollowingStatus::Moving;
	}
}

// ---------------------------------------------------------------------------
// FDoorwayCrossing
//
// An AI agent never stands still in a doorway, enemy and follower alike: the
// doorway is carved out of the navmesh, and a path started from there can
// begin on either side of the door. Two rules, one place:
//   - Stops and new moves wait while IsCrossing: the agent is walking the
//     nav link, or walking out of the doorway.
//   - A move that ends inside the doorway of the door the agent last crossed
//     (its acceptance radius reached halfway through, or a goal in the
//     doorway) walks straight on to the link end it was heading to.
//
// Owned by the AI controller, which forwards IDoorAgent::HandleDoorCrossing,
// OnMoveCompleted and its tick.
// ---------------------------------------------------------------------------
struct NEVERGONE_API FDoorwayCrossing
{
	// The agent starts walking the nav link through Door and comes out at Exit.
	void HandleDoorCrossing(ADoor* Door, const FVector& Exit);

	// One of the controller's moves finished.
	void HandleMoveCompleted(const AAIController& Controller, FAIRequestID RequestID, const FPathFollowingResult& Result);

	// Starts the walk out of the doorway a finished move asked for. Call from
	// the controller's tick, while it is not frozen.
	void Tick(AAIController& Controller);

	// True while walking a nav link, or walking out of a doorway.
	bool IsCrossing(const AAIController& Controller) const;

	// Forgets the crossing, for an agent that stops being in control of its
	// body (possession ends, a capture, death).
	void Reset();

	// True when a finished move leaves the agent to walk out of the doorway:
	// it arrived (an aborted move was stopped on purpose) inside the doorway
	// of the door it crossed.
	static bool ShouldWalkOut(bool bCrossedDoor, bool bMoveSucceeded, bool bInDoorway);

private:

	// Walks straight to the exit of the door last crossed.
	void LeaveDoorway(AAIController& Controller);

	// The door whose nav link the agent last walked, and where it comes out
	// on the far side.
	TWeakObjectPtr<ADoor> CrossingDoor;

	FVector CrossingExit = FVector::ZeroVector;

	// A move ended in the doorway: the next tick walks out of it.
	bool bLeaveDoorwayPending = false;

	// The move walking out of the doorway, while it runs.
	FAIRequestID DoorwayExitRequest;
};
