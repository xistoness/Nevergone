// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/ExplorationEnemyTypes.h"

// ---------------------------------------------------------------------------
// FScentBaitLife
//
// How long a scent bait stays on the ground. It starts with its ground life,
// spent while nobody eats it. The first enemy to start eating replaces that
// with the eating time; later eaters change nothing. Either running out ends
// the bait. Plain struct so it can be tested without a world; the bait
// advances it with its own dilated delta time, so a capture freeze stops it.
// ---------------------------------------------------------------------------
struct NEVERGONE_API FScentBaitLife
{
	void Start(float GroundSeconds);

	// True when this call started the eating time (the first eater).
	bool StartEating(float EatSeconds);

	// Spends DeltaTime. True once the bait is used up or left too long.
	bool Advance(float DeltaTime);

	bool IsBeingEaten() const { return bBeingEaten; }

	float GetSecondsLeft() const { return SecondsLeft; }

private:

	float SecondsLeft = 0.0f;

	bool bBeingEaten = false;
};

// ---------------------------------------------------------------------------
// ScentBaitRules
//
// Where a scent bait sits among an enemy's priorities, kept pure so it can
// be tested. The controller applies them.
// ---------------------------------------------------------------------------
namespace ScentBaitRules
{
	// The states the smell of a bait pulls an enemy out of: patrol (standing
	// guard too) and an investigation. A chase and a suspicion ignore it, the
	// capture states cannot act, and Idle is only the moment before the
	// first patrol.
	NEVERGONE_API bool CanBeBaited(EExplorationEnemyState State);

	// Sight works while walking to the bait and stops once eating.
	NEVERGONE_API bool CanSee(EExplorationEnemyState State, bool bEating);

	// Hearing stops for the whole bait: on the way to it the enemy is
	// already inside its scent, and a noise that sent it to investigate
	// would have it smell the bait again at once.
	NEVERGONE_API bool CanReactToNoise(EExplorationEnemyState State);
}
