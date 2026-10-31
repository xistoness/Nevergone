// Copyright Xyzto Works

#include "Types/ScentBaitRules.h"

// ---------------------------------------------------------------------------
// FScentBaitLife
// ---------------------------------------------------------------------------

void FScentBaitLife::Start(float GroundSeconds)
{
	SecondsLeft = GroundSeconds;
	bBeingEaten = false;
}

bool FScentBaitLife::StartEating(float EatSeconds)
{
	if (bBeingEaten)
	{
		return false;
	}

	bBeingEaten = true;
	SecondsLeft = EatSeconds;

	return true;
}

bool FScentBaitLife::Advance(float DeltaTime)
{
	SecondsLeft -= DeltaTime;

	return SecondsLeft <= 0.0f;
}

// ---------------------------------------------------------------------------
// ScentBaitRules
// ---------------------------------------------------------------------------

bool ScentBaitRules::CanBeBaited(EExplorationEnemyState State)
{
	// Not Idle: that is only the moment before the first patrol starts,
	// which would override the bait.
	return State == EExplorationEnemyState::Patrol ||
		State == EExplorationEnemyState::Investigate;
}

bool ScentBaitRules::CanSee(EExplorationEnemyState State, bool bEating)
{
	return State != EExplorationEnemyState::Baited || !bEating;
}

bool ScentBaitRules::CanReactToNoise(EExplorationEnemyState State)
{
	// A noise never confirms a target, it only moves the enemy to look. A
	// chase keeps its target, a suspicious enemy is already looking at one
	// and holds the noise instead, a baited one stays with the bait, and a
	// captured, downed or dead one cannot react.
	return State == EExplorationEnemyState::Idle ||
		State == EExplorationEnemyState::Patrol ||
		State == EExplorationEnemyState::Investigate;
}
