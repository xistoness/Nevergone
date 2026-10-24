// Copyright Xyzto Works

#include "Types/ExplorationSuspicion.h"

void FExplorationSuspicion::Reset()
{
	*this = FExplorationSuspicion();
}

void FExplorationSuspicion::Open(const FVector& TargetLocation)
{
	SightLossSeconds = 0.0f;
	bSightLost = false;

	LastSeenLocation = TargetLocation;

	bHasHeldNoise = false;
}

void FExplorationSuspicion::AddContact(float DeltaTime)
{
	ContactSeconds += DeltaTime;
}

void FExplorationSuspicion::RestartContact()
{
	ContactSeconds = 0.0f;
	SightLossSeconds = 0.0f;
	bSightLost = false;

	bHasHeldNoise = false;
}

EExplorationSuspicionStep FExplorationSuspicion::Advance(bool bTargetInView, const FVector& TargetLocation,
	float DeltaTime, float ToleranceSeconds)
{
	if (bTargetInView)
	{
		const bool bWasLost = bSightLost;

		bSightLost = false;
		SightLossSeconds = 0.0f;

		LastSeenLocation = TargetLocation;

		ContactSeconds += DeltaTime;

		if (bWasLost)
		{
			// Seen after the noise: the sighting is the most recent stimulus.
			bHasHeldNoise = false;

			return EExplorationSuspicionStep::Regained;
		}

		return EExplorationSuspicionStep::Watching;
	}

	const bool bJustLost = !bSightLost;

	bSightLost = true;
	SightLossSeconds += DeltaTime;

	if (SightLossSeconds >= ToleranceSeconds)
	{
		return EExplorationSuspicionStep::Expired;
	}

	return bJustLost ? EExplorationSuspicionStep::LostSight : EExplorationSuspicionStep::Tolerating;
}

void FExplorationSuspicion::HoldNoise(const FVector& NoiseLocation)
{
	bHasHeldNoise = true;

	HeldNoiseLocation = NoiseLocation;
}

void FExplorationSuspicion::DiscardHeldNoise()
{
	bHasHeldNoise = false;
}

FVector FExplorationSuspicion::ResolveStimulus() const
{
	return bHasHeldNoise ? HeldNoiseLocation : LastSeenLocation;
}
