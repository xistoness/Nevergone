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

float DoorRules::ChooseSwingSign(float OpenerSide, const FVector& LeafCentre, float OpenAngleDegrees, const FVector& Axis)
{
	// Where the middle of the leaf ends up for a positive yaw. Judged at 90
	// at most: near 180 the leaf comes back to the doorway plane and its
	// side no longer tells the direction.
	const float JudgedDegrees = FMath::Min(OpenAngleDegrees, 90.f);

	const FVector Swung = FQuat(FVector::UpVector, FMath::DegreesToRadians(JudgedDegrees)).RotateVector(LeafCentre);

	const float SwungSide = FVector::DotProduct(Swung, Axis);

	// The positive yaw when it lands on the other side of the opener.
	return (SwungSide * OpenerSide <= 0.f) ? 1.f : -1.f;
}
