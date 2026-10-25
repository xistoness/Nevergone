// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"

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
