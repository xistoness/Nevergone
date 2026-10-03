// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Types/ExplorationEnemyTypes.h"
#include "ExplorationEnemyController.generated.h"

class AExplorationEnemyCharacter;
class UExplorationEnemyDefinition;
class UExplorationPerceptionComponent;
enum class EExplorationSenseChannel : uint8;

// ---------------------------------------------------------------------------
// AExplorationEnemyController
//
// The brain of a daytime enemy. Plain C++ state machine, no behaviour tree and
// no StateTree asset: a handful of states with fixed rules is not enough
// complexity to justify an asset that has to be opened in the editor to be
// read.
//
// State rules:
//   Patrol      Stationary scans in place, Route walks its waypoints, Wander
//               picks random reachable points around home.
//   Investigate Something was heard, or the survivor was seen and then lost.
//               Turns to face the stimulus first, then walks to the last
//               known location, looks around, gives up. Facing first matters
//               because sight follows the pawn's rotation: a noise made
//               across the room is looked at from where the enemy stands,
//               instead of only once it has walked around the furniture.
//   Chase       The survivor is confirmed. Moves at ChaseSpeed and repaths.
//               Losing sight does not end the chase until MemorySeconds pass.
//   Catch       The survivor is within CatchDistance and
//               UExcursionEncounterSubsystem accepted the capture. The
//               capture sequence drives the body; the brain waits for
//               BeginKnockdown, Kill or FinishFeeding.
//   KnockedDown A bitten survivor pushed the enemy down. It lies for the
//               type's KnockdownSeconds, then chases that survivor again.
//   Dead        Killed by a safeguard. Never acts again.
//
// While a player capture freezes the world, this controller's time dilation
// is zero and Tick does nothing.
//
// Escalation differs per sense: hearing only ever causes Investigate, sight
// needs TimeToConfirmSeconds of continuous contact before it becomes a chase,
// and smell escalates immediately because by then the survivor is already
// close enough to touch.
//
// Targets: every survivor body counts (the player's survivor and recruited
// followers), but the enemy works on one at a time. Sight confirms the
// closest survivor in view and sticks with it while it stays in view; a
// chase sticks with its target until it is lost, whoever else shows up.
//
// The controller never resolves a capture. It reports the catch to
// UExcursionEncounterSubsystem, which decides the outcome and calls back.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API AExplorationEnemyController : public AAIController
{
	GENERATED_BODY()

public:

	AExplorationEnemyController();

	// --- Capture callbacks, called by UExcursionEncounterSubsystem ---

	// A bitten survivor pushed the enemy down. Lies for Seconds, then chases
	// ChaseTargetAfter again with the tracking grace window open.
	void BeginKnockdown(float Seconds, AActor* ChaseTargetAfter);

	// Killed by a safeguard.
	void Kill();

	// Done devouring a follower. Goes to the last place it sensed the
	// player, however old, or back to patrol when it never sensed them.
	void FinishFeeding();

	virtual void OnPossess(APawn* InPawn) override;

	virtual void OnUnPossess() override;

	virtual void Tick(float DeltaTime) override;

protected:

	// How close the pawn must get to a patrol destination to count as arrived.
	UPROPERTY(EditDefaultsOnly, Category = "Exploration AI", meta = (ClampMin = "1.0"))
	float MoveAcceptanceRadius = 50.0f;

	// How often a chase re-issues its move order. The survivor is moving, so
	// a path computed once goes stale; repathing every frame is wasteful.
	UPROPERTY(EditDefaultsOnly, Category = "Exploration AI", meta = (ClampMin = "0.05"))
	float ChaseRepathInterval = 0.25f;

private:

	UPROPERTY(VisibleAnywhere, Category = "Exploration AI")
	TObjectPtr<UExplorationPerceptionComponent> EnemyPerceptionComponent = nullptr;

	// --- State machine ---

	void EnterState(EExplorationEnemyState NewState);

	void TickPatrol(float DeltaTime);
	void TickInvestigate(float DeltaTime);
	void TickChase(float DeltaTime);
	void TickKnockdown(float DeltaTime);

	// --- Patrol helpers ---

	void StartPatrol();
	void TickStationaryScan(float DeltaTime);
	void AdvanceRoute();
	void PickWanderDestination();

	// True once the pawn is within MoveAcceptanceRadius of its current goal, or
	// once the move request has finished for any reason.
	bool HasReachedDestination() const;

	// --- Investigate ---

	// Resets the travel/search sub-state. Called on entering Investigate and
	// whenever a new stimulus re-targets an investigation in progress.
	void BeginInvestigation();

	// Turns the pawn towards FaceTarget. Returns true when it is facing it,
	// close enough, or has spent its time budget trying.
	bool UpdateStimulusFacing(float DeltaTime);

	// Shared by Wander and the search phase. Returns false when no point was
	// found or the move request failed.
	bool MoveToRandomReachablePoint(const FVector& Origin, float Radius);
	
	// --- Perception handlers ---

	void HandleSurvivorSensed(AActor* Survivor, const FVector& StimulusLocation, EExplorationSenseChannel Channel);
	void HandleSurvivorLost(AActor* Survivor);

	// --- Cached context ---

	TWeakObjectPtr<AExplorationEnemyCharacter> ControlledEnemy;

	UPROPERTY()
	TObjectPtr<const UExplorationEnemyDefinition> Definition = nullptr;

	TWeakObjectPtr<AActor> ChaseTarget;

	// Rotation the pawn held on possess. Anchor for the stationary scan and the
	// pose a giving-up enemy returns to.
	FRotator HomeRotation = FRotator::ZeroRotator;

	// --- Timers ---

	// Continuous seconds of sight contact with ConfirmTarget, reset whenever
	// sight drops or the enemy starts confirming someone else.
	float SightContactSeconds = 0.0f;

	// Survivor whose sight contact is being accumulated.
	TWeakObjectPtr<AActor> ConfirmTarget;

	// Seconds left on the ground after a bite.
	float KnockdownLeft = 0.0f;

	// Survivor to chase again once back on its feet.
	TWeakObjectPtr<AActor> KnockdownChaseTarget;

	// Seconds since the survivor was last sensed by any channel while chasing.
	float TimeSinceLastContact = 0.0f;

	// Seconds spent standing at an investigated location.
	float InvestigateElapsed = 0.0f;

	// Countdown for the pause at a waypoint or wander destination.
	float WaitRemaining = 0.0f;

	float TimeSinceRepath = 0.0f;

	// --- Patrol state ---

	int32 CurrentWaypointIndex = 0;

	// Direction of the stationary scan sweep: 1 or -1.
	float ScanDirection = 1.0f;

	// Current yaw offset from HomeRotation during a stationary scan.
	float ScanOffsetDegrees = 0.0f;

	// Placed pawns are possessed inside PostInitializeComponents, before any
	// BeginPlay has run. Patrol waits for the first tick so the pawn's home
	// location and the navigation system are both ready.
	bool bPatrolStartPending = false;

	// Point the search is centred on: the last known location at the moment
	// the investigation began.
	FVector SearchAnchor = FVector::ZeroVector;

	bool bInvestigateMoveIssued = false;

	// The enemy is standing still, turning towards the stimulus.
	bool bFacingStimulus = false;

	float FaceStimulusElapsed = 0.0f;

	FVector FaceTarget = FVector::ZeroVector;

	bool bIsSearching = false;
};