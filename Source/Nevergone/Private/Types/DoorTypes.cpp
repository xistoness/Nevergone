// Copyright Xyzto Works

#include "Types/DoorTypes.h"

EDoorAgentPassage DoorRules::ResolveAgentPassage(EDoorState State)
{
	switch (State)
	{
	case EDoorState::Open:
		return EDoorAgentPassage::Pass;

	case EDoorState::Closed:
		return EDoorAgentPassage::OpenFirst;

	case EDoorState::Locked:
	default:
		// A locked key door is a wall for AI: agents never use the party's key.
		return EDoorAgentPassage::Blocked;
	}
}

bool DoorRules::IsAgentLinkEnabled(EDoorState State)
{
	return ResolveAgentPassage(State) != EDoorAgentPassage::Blocked;
}
