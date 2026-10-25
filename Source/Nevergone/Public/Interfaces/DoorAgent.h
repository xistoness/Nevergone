// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DoorAgent.generated.h"

class ADoor;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDoorAgent : public UInterface
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// IDoorAgent
//
// An AI controller that does something of its own at a door. ADoor tells the
// controller (the owner of the path following component). A door that will
// not open pauses the agent's move at the nav link; a controller that does
// not implement this just waits there until somebody opens the door.
// ---------------------------------------------------------------------------
class NEVERGONE_API IDoorAgent
{
	GENERATED_BODY()

public:

	// The agent's move is paused at Door, which is barred (latched). Called
	// again for every new move request that reaches the same door.
	virtual void HandleDoorBarred(ADoor* Door) = 0;

	// The agent starts walking the nav link through Door and comes out at
	// Exit, the link end on the far side.
	virtual void HandleDoorCrossing(ADoor* Door, const FVector& Exit) {}
};
