// Copyright Xyzto Works

#include "EngineTools/AINavLinks.h"

#include "Nevergone.h"
#include "Actors/Door.h"

namespace DoorwayCrossingConstants
{
	// How close to the link end the walk out of a doorway gets.
	static constexpr float DoorwayExitAcceptanceRadius = 20.0f;
}

void FDoorwayCrossing::HandleDoorCrossing(ADoor* Door, const FVector& Exit)
{
	CrossingDoor = Door;
	CrossingExit = Exit;
}

void FDoorwayCrossing::HandleMoveCompleted(const AAIController& Controller, FAIRequestID RequestID,
	const FPathFollowingResult& Result)
{
	// Out of the doorway, or stopped on the way out: that crossing is done.
	if (DoorwayExitRequest.IsValid() && RequestID == DoorwayExitRequest)
	{
		DoorwayExitRequest = FAIRequestID::InvalidRequest;
		CrossingDoor = nullptr;

		return;
	}

	const ADoor* Door = CrossingDoor.Get();

	// A stop halfway through is postponed before it gets here.
	if (ShouldWalkOut(Door != nullptr, Result.IsSuccess(), Door && Door->IsInDoorway(Controller.GetPawn())))
	{
		bLeaveDoorwayPending = true;

		return;
	}

	CrossingDoor = nullptr;
}

void FDoorwayCrossing::Tick(AAIController& Controller)
{
	if (bLeaveDoorwayPending)
	{
		bLeaveDoorwayPending = false;

		LeaveDoorway(Controller);
	}
}

bool FDoorwayCrossing::IsCrossing(const AAIController& Controller) const
{
	const bool bLeavingDoorway = DoorwayExitRequest.IsValid() &&
		Controller.GetCurrentMoveRequestID() == DoorwayExitRequest &&
		Controller.GetMoveStatus() != EPathFollowingStatus::Idle;

	return bLeaveDoorwayPending || bLeavingDoorway || NevergoneAI::IsWalkingNavLink(Controller);
}

void FDoorwayCrossing::Reset()
{
	CrossingDoor = nullptr;
	bLeaveDoorwayPending = false;
	DoorwayExitRequest = FAIRequestID::InvalidRequest;
}

bool FDoorwayCrossing::ShouldWalkOut(bool bCrossedDoor, bool bMoveSucceeded, bool bInDoorway)
{
	return bCrossedDoor && bMoveSucceeded && bInDoorway;
}

void FDoorwayCrossing::LeaveDoorway(AAIController& Controller)
{
	if (!CrossingDoor.IsValid() || !Controller.GetPawn())
	{
		return;
	}

	// Straight along the link: the doorway has no navmesh to path on.
	const EPathFollowingRequestResult::Type Result = Controller.MoveToLocation(
		CrossingExit,
		DoorwayCrossingConstants::DoorwayExitAcceptanceRadius,
		/*bStopOnOverlap=*/false,
		/*bUsePathfinding=*/false);

	if (Result == EPathFollowingRequestResult::RequestSuccessful)
	{
		DoorwayExitRequest = Controller.GetCurrentMoveRequestID();

		UE_LOG(LogNevergone, Verbose,
			TEXT("[DoorwayCrossing] %s stopped in the doorway of %s and walks out of it to %s"),
			*GetNameSafe(Controller.GetPawn()), *GetNameSafe(CrossingDoor.Get()), *CrossingExit.ToCompactString());
	}
	else
	{
		// Already at the exit. AlreadyAtGoal completes the move at once,
		// inside the call, and that completion asked for another walk out.
		bLeaveDoorwayPending = false;
		CrossingDoor = nullptr;
	}
}
