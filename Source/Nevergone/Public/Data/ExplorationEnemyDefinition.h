// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/ExplorationEnemyTypes.h"
#include "ExplorationEnemyDefinition.generated.h"

class UNoiseDefinition;

// ---------------------------------------------------------------------------
// UExplorationEnemyDefinition
//
// Everything that makes one kind of exploration enemy different from another:
// how fast it moves, how easily it notices the survivor, how it occupies the
// level, how it looks, and how dangerous a capture is.
//
// AExplorationEnemyCharacter reads the body-facing values (collision, speeds,
// visuals). AExplorationEnemyController reads the behaviour values (perception,
// patrol, memory). UExcursionEncounterSubsystem reads the capture values.
//
// Authoring a new enemy type must never require a new C++ class.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UExplorationEnemyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	// --- Identity ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	// --- Visuals ---

	// Art options for this type. One is resolved per spawned actor.
	// An empty list leaves whatever mesh the Blueprint subclass already has.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TArray<FExplorationEnemyVisualVariant> VisualVariants;

	// Capsule of this type. Applied to the character on spawn, and the skeletal
	// mesh is pushed down by CapsuleHalfHeight so feet stay on the floor
	// regardless of how tall the variant is.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", meta = (ClampMin = "1.0"))
	float CapsuleRadius = 34.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", meta = (ClampMin = "1.0"))
	float CapsuleHalfHeight = 88.0f;

	// --- Movement ---

	// Speed while patrolling, investigating, or standing guard.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float PatrolSpeed = 120.0f;

	// Speed once the survivor is confirmed. This is the number that decides
	// whether running away is ever a real option against this type.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float ChaseSpeed = 450.0f;

	// Degrees per second the actor turns toward its movement direction.
	// Low values make a type easy to slip behind.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "10.0"))
	float TurnRateDegrees = 240.0f;

	// --- Perception: sight ---

	// Distance at which the survivor can first be seen, in centimetres.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception|Sight", meta = (ClampMin = "0.0"))
	float SightRadius = 1200.0f;

	// Distance at which an already-seen survivor is dropped from sight.
	// Must be greater than SightRadius or the enemy flickers between states.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception|Sight", meta = (ClampMin = "0.0"))
	float LoseSightRadius = 1600.0f;

	// Half-angle of the vision cone. 60 means a 120 degree field of view.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception|Sight", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float SightHalfAngleDegrees = 60.0f;

	// --- Perception: hearing ---

	// Radius that picks up noise events reported by footsteps, doors, and
	// distraction items.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception|Hearing", meta = (ClampMin = "0.0"))
	float HearingRadius = 900.0f;

	// Fraction of the hearing range kept for each occluding object (wall,
	// closed door) between this enemy and the noise. Applied once per object:
	// 0.5 through two walls leaves 25% of the range.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception|Hearing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HearingOcclusionPerWall = 0.5f;

	// --- Perception: smell ---

	// Smell is proximity only: it ignores walls, cones, and line of sight, and
	// exists so that hiding right next to a type that has it is not safe.
	// The engine ships no smell sense, so the controller polls this radius.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception|Smell")
	bool bCanSmell = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception|Smell", meta = (ClampMin = "0.0", EditCondition = "bCanSmell"))
	float SmellRadius = 300.0f;

	// --- Perception: timing ---

	// How long a stimulus must hold before Investigate escalates to Chase.
	// This is the reaction window the player plays against: a high value makes
	// a type forgiving, a low value makes it lethal.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception|Timing", meta = (ClampMin = "0.0"))
	float TimeToConfirmSeconds = 1.5f;

	// How long the survivor being confirmed may stay out of view without
	// ending the suspicion. Covers a survivor flickering at a corner or at the
	// edge of the cone: meanwhile the enemy holds still facing the last seen
	// point, and the confirmation neither grows nor resets.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception|Timing", meta = (ClampMin = "0.0"))
	float SuspicionSightLossToleranceSeconds = 0.75f;

	// Longest the enemy keeps running toward the last known location after
	// losing the survivor. Arriving there ends the chase earlier.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception|Timing", meta = (ClampMin = "0.0"))
	float MemorySeconds = 6.0f;
	
	// Seconds after losing contact during a chase in which the enemy keeps
	// tracking the survivor's real position. Covers the survivor breaking line
	// of sight at a door and turning immediately. Chase only: an enemy that has
	// not confirmed the survivor never tracks through walls.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception|Timing", meta = (ClampMin = "0.0"))
	float TrackingGraceSeconds = 1.5f;

	// How long the enemy searches around the last known location, counted
	// from arrival, before giving up and returning to patrol.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception|Timing", meta = (ClampMin = "0.0"))
	float InvestigateDurationSeconds = 8.0f;

	// --- Behaviour ---

	// Turn towards a stimulus before walking to it. Without this the enemy
	// only faces where its path goes, so a noise made behind a sofa is
	// investigated with the enemy staring at the sofa the whole way round.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour")
	bool bFaceStimulusBeforeMoving = true;

	// How close to the stimulus direction counts as facing it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "1.0", ClampMax = "90.0", EditCondition = "bFaceStimulusBeforeMoving"))
	float FaceStimulusToleranceDegrees = 10.0f;

	// Safety cap on the turn. Without it a slow enemy asked to turn 180
	// degrees would stand still long enough to look broken.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0", EditCondition = "bFaceStimulusBeforeMoving"))
	float FaceStimulusMaxSeconds = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour")
	EExplorationPatrolMode PatrolMode = EExplorationPatrolMode::Stationary;

	// Radius around the spawn point used by Wander mode.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0", EditCondition = "PatrolMode == EExplorationPatrolMode::Wander"))
	float WanderRadius = 800.0f;
	
	// Radius around the last known location that Investigate searches.
	// Small values read as a confused enemy, large ones as a hunting one.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float SearchRadius = 400.0f;

	// A Stationary enemy that never turns is a static obstacle the player walks
	// around once and forgets. Sweeping makes it a timing problem instead.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (EditCondition = "PatrolMode == EExplorationPatrolMode::Stationary"))
	bool bScanWhileStationary = true;

	// Half-angle of the stationary sweep, measured from the placed rotation.
	// 45 means the enemy covers 90 degrees of corridor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0", ClampMax = "180.0", EditCondition = "PatrolMode == EExplorationPatrolMode::Stationary && bScanWhileStationary"))
	float ScanHalfAngleDegrees = 45.0f;

	// Pause at each waypoint (Route), each wander destination (Wander), or each
	// end of the sweep (Stationary).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0"))
	float WaitAtDestinationSeconds = 2.0f;

	// --- Capture ---

	// Distance at which the chase ends and the capture starts.
	// 100 units is roughly one metre.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "1.0"))
	float CatchDistance = 100.0f;

	// Chance, in percent, that a capture without a safeguard kills the
	// survivor instead of biting them. Strength lowers it and earlier bites
	// raise it; the formula lives in UExcursionEncounterSubsystem and its
	// constants in Project Settings > Excursion Encounter.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "Percent"))
	float BaseDeathChance = 30.0f;

	// Seconds the enemy stays down after a bitten survivor pushes it away:
	// the survivor's window to escape.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "0.0", Units = "Seconds"))
	float KnockdownSeconds = 4.0f;

	// Seconds the enemy spends devouring a follower. The world keeps running
	// meanwhile.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "0.0", Units = "Seconds"))
	float DevourFollowerSeconds = 5.0f;

	// Distance between the enemy and the victim while a capture plays.
	// Match it to the paired animations.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "0.0"))
	float CaptureStandOffDistance = 70.0f;

	// Noise of the struggle when the player's survivor is caught. Enemies
	// frozen during the capture still hear it and react once it ends.
	// Empty for a silent capture.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture")
	TObjectPtr<UNoiseDefinition> CaptureNoise = nullptr;

	// The survivor defends with the safeguard and kills the enemy.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture|Animation")
	FExcursionCaptureAnimation SafeguardAnimation;

	// The enemy bites; the survivor pushes it to the ground.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture|Animation")
	FExcursionCaptureAnimation BiteAnimation;

	// The enemy takes the survivor down and devours them.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture|Animation")
	FExcursionCaptureAnimation KillAnimation;

	// The enemy devours a follower. Should loop: DevourFollowerSeconds, not
	// the montage length, decides how long it lasts.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture|Animation")
	FExcursionCaptureAnimation DevourFollowerAnimation;

	// --- Signals ---
	//
	// How the enemy shows its state with no UI, played by
	// UExplorationEnemySignalComponent. They read the same whoever the target
	// is, the player's survivor or a follower. Every field is optional.

	// A survivor came into view: the enemy stops and turns towards them while
	// the confirmation window is open.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signals")
	FExplorationEnemySignal SuspicionSignal;

	// Walking to a stimulus and searching. The sound loops for as long as the
	// investigation lasts, whether or not the asset itself loops, and pauses
	// with the world.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signals")
	FExplorationEnemySignal InvestigationSignal;

	// The chase starts: a scream that other enemies hear as a noise.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signals")
	FExplorationEnemyNoiseSignal ChaseSignal;

	// Back to patrol after an investigation or a chase.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signals")
	FExplorationEnemySignal GiveUpSignal;

	// One knock at a latched door, every KnockIntervalSeconds. The noise
	// sounds at the door and must be tagged Noise.DoorKnock: other enemies
	// come to investigate it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signals")
	FExplorationEnemyNoiseSignal KnockSignal;

	// The latched door gives way. The noise sounds at the door and must be
	// tagged Noise.DoorBreak.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Signals")
	FExplorationEnemyNoiseSignal BreakSignal;

	// --- Doors ---

	// Time between knocks at a latched door. Match it to the knock montage.
	// How long the door holds is the door's BreakSeconds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Doors", meta = (ClampMin = "0.1", Units = "s"))
	float KnockIntervalSeconds = 1.2f;

#if WITH_EDITOR
	// Catches the two configuration mistakes that produce silent, hard-to-debug
	// behaviour: a lose-sight radius inside the sight radius (state flicker),
	// and a chase slower than the patrol speed.
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};