// Copyright Xyzto Works

#include "Types/DoorTypes.h"

namespace DoorKnockConstants
{
	// A zero interval would knock every frame.
	static constexpr float MinKnockIntervalSeconds = 0.1f;
}

FDoorKnock::FStep FDoorKnock::Advance(float DeltaSeconds, float KnockIntervalSeconds, float BreakSeconds)
{
	FStep Step;

	if (!bStarted)
	{
		bStarted = true;
		ElapsedSeconds = 0.0f;
		NextKnockAtSeconds = 0.0f;
	}

	ElapsedSeconds += FMath::Max(DeltaSeconds, 0.0f);

	if (ElapsedSeconds >= BreakSeconds)
	{
		Step.bBreak = true;

		return Step;
	}

	if (ElapsedSeconds >= NextKnockAtSeconds)
	{
		Step.bKnock = true;

		const float Interval = FMath::Max(KnockIntervalSeconds, DoorKnockConstants::MinKnockIntervalSeconds);

		// Keeps the rhythm; a long frame plays one knock, not several.
		NextKnockAtSeconds += Interval;

		if (NextKnockAtSeconds <= ElapsedSeconds)
		{
			NextKnockAtSeconds = ElapsedSeconds + Interval;
		}
	}

	return Step;
}

void FDoorKnock::Reset()
{
	ElapsedSeconds = 0.0f;
	NextKnockAtSeconds = 0.0f;
	bStarted = false;
}

EDoorAgentPassage DoorRules::ResolveAgentPassage(EDoorState State)
{
	switch (State)
	{
	case EDoorState::Open:
	case EDoorState::Broken:
		return EDoorAgentPassage::Pass;

	case EDoorState::Closed:
		return EDoorAgentPassage::OpenFirst;

	case EDoorState::Latched:
		// No agent works the latch: an enemy knocks, anyone else waits.
		return EDoorAgentPassage::Barred;

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

bool DoorRules::IsOnLatchSide(EDoorLatchSide LatchSide, float OpenerSide)
{
	switch (LatchSide)
	{
	case EDoorLatchSide::Front:
		return OpenerSide > 0.f;

	case EDoorLatchSide::Back:
		return OpenerSide < 0.f;

	case EDoorLatchSide::None:
	default:
		return false;
	}
}

bool DoorRules::CanHoldInteract(EDoorState State, bool bOnLatchSide)
{
	return bOnLatchSide &&
		(State == EDoorState::Closed || State == EDoorState::Open || State == EDoorState::Latched);
}

EDoorInteraction DoorRules::ResolveInteraction(EDoorState State, bool bOnLatchSide, bool bHold)
{
	const bool bLatchHold = bHold && CanHoldInteract(State, bOnLatchSide);

	switch (State)
	{
	case EDoorState::Locked:
		return EDoorInteraction::UseKey;

	case EDoorState::Closed:
		return bLatchHold ? EDoorInteraction::Latch : EDoorInteraction::Open;

	case EDoorState::Open:
		return bLatchHold ? EDoorInteraction::CloseAndLatch : EDoorInteraction::Close;

	case EDoorState::Latched:
		if (!bOnLatchSide)
		{
			return EDoorInteraction::Jolt;
		}

		return bLatchHold ? EDoorInteraction::Unlatch : EDoorInteraction::UnlatchAndOpen;

	case EDoorState::Broken:
	default:
		return EDoorInteraction::None;
	}
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

bool DoorRules::DoesDoorBlockCapture(EDoorState State)
{
	switch (State)
	{
	case EDoorState::Locked:
	case EDoorState::Closed:
	case EDoorState::Latched:
		return true;

	case EDoorState::Open:
	case EDoorState::Broken:
	default:
		return false;
	}
}

bool DoorRules::DoesSegmentCrossDoorway(const FVector& LocalFrom, const FVector& LocalTo, const FVector& BoxExtent)
{
	// Strictly on opposite sides of the door plane.
	if (LocalFrom.X * LocalTo.X >= 0.f)
	{
		return false;
	}

	// Where the segment meets the plane.
	const float Alpha = LocalFrom.X / (LocalFrom.X - LocalTo.X);

	const FVector Crossing = FMath::Lerp(LocalFrom, LocalTo, Alpha);

	return FMath::Abs(Crossing.Y) <= BoxExtent.Y && FMath::Abs(Crossing.Z) <= BoxExtent.Z;
}

float DoorRules::GetJoltOffset(float ElapsedSeconds, float DurationSeconds, float AngleDegrees)
{
	if (DurationSeconds <= 0.f || ElapsedSeconds <= 0.f || ElapsedSeconds >= DurationSeconds)
	{
		return 0.f;
	}

	const float Half = DurationSeconds * 0.5f;

	// Out to the catch, then back.
	const float Fraction = ElapsedSeconds <= Half
		? ElapsedSeconds / Half
		: 1.f - (ElapsedSeconds - Half) / Half;

	return AngleDegrees * Fraction;
}
