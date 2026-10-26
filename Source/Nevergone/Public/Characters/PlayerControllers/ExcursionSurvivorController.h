// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EngineTools/AINavLinks.h"
#include "Interfaces/DoorAgent.h"
#include "ExcursionSurvivorController.generated.h"

class ACharacter;
class ADoor;
class AExcursionSurvivorNPC;
class UExplorationModeComponent;
enum class EExcursionSurvivorState : uint8;

// ---------------------------------------------------------------------------
// AExcursionSurvivorController
//
// The brain of a rescued survivor. It only acts while the survivor is
// Following, and does two things:
//
//   Follow  Walks toward the player-controlled survivor and stops at
//           StopDistance. Starts again only past ResumeDistance, so small
//           steps by the player do not make the follower twitch.
//   Mimic   Copies the player's crouch and sprint after MimicDelaySeconds.
//           Speed comes from the follower's own UExplorationModeComponent,
//           the same component with the same numbers as the player's, so the
//           gap does not close while the player walks: the follower only
//           catches up when the player stops. Running to catch up would make
//           noise the player did not choose.
//
// While Waiting the follower stops, keeps the stance it had and lets go of
// the sprint. Stamina and panting belong to its own component and keep
// working in both states.
//
// The follower never stands still in a doorway: the doorway is carved out of
// the navmesh, and a path started from there can begin on either side of the
// door. Told to wait halfway through, it finishes crossing first. A follow
// move that reaches StopDistance halfway through walks straight on to the
// link end it was heading to (the door tells it, IDoorAgent). Both rules
// live in FDoorwayCrossing, shared with the enemies.
//
// At a latched door the follower waits, its move paused by the door, until
// somebody opens it.
//
// The leader is read from player 0 every tick instead of cached, so swapping
// the party leader mid-excursion needs no notification.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API AExcursionSurvivorController : public AAIController, public IDoorAgent
{
	GENERATED_BODY()

public:

	AExcursionSurvivorController();

	virtual void OnPossess(APawn* InPawn) override;

	virtual void OnUnPossess() override;

	virtual void Tick(float DeltaTime) override;

	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	// --- IDoorAgent ---

	// Nothing to do at a latched door: the follower waits there.
	virtual void HandleDoorBarred(ADoor* Door) override {}

	virtual void HandleDoorCrossing(ADoor* Door, const FVector& Exit) override;

protected:

	// Distance to the leader at which the follower stops walking.
	UPROPERTY(EditDefaultsOnly, Category = "Survivor AI", meta = (ClampMin = "0.0"))
	float StopDistance = 200.0f;

	// Distance at which a stopped follower starts walking again. Kept above
	// StopDistance so the follower does not start and stop on every step.
	UPROPERTY(EditDefaultsOnly, Category = "Survivor AI", meta = (ClampMin = "0.0"))
	float ResumeDistance = 300.0f;

	// Reaction time before a change of stance is copied. Reads as a person
	// reacting instead of a mirror.
	UPROPERTY(EditDefaultsOnly, Category = "Survivor AI", meta = (ClampMin = "0.0"))
	float MimicDelaySeconds = 0.3f;

private:

	void HandleSurvivorStateChanged(AExcursionSurvivorNPC* Survivor, EExcursionSurvivorState NewState);

	void TickFollow(ACharacter* Leader);

	void TickMimic(float DeltaTime, const ACharacter* Leader);

	// Pushes a stance into the follower's pawn and exploration mode. Safe to
	// call every tick: it only acts on what differs.
	void ApplyStance(bool bCrouched, bool bSprinting);

	// Re-reads everything cached about the leader when player 0's pawn is a
	// different one than last tick.
	void RefreshLeader(ACharacter* Leader);

	ACharacter* GetLeader() const;

	// True while walking a nav link, or walking out of a doorway.
	bool IsCrossingDoorway() const;

	// A stop asked while crossing a doorway: done once the pawn is out.
	bool bStopAfterNavLink = false;

	// Never standing still in a doorway, same rule as the enemies.
	FDoorwayCrossing DoorwayCrossing;

	TWeakObjectPtr<AExcursionSurvivorNPC> ControlledSurvivor;

	// The follower's own moveset. Sprint goes through it so stamina and
	// panting apply to the follower exactly as they do to the player.
	TWeakObjectPtr<UExplorationModeComponent> SurvivorMode;

	TWeakObjectPtr<ACharacter> CachedLeader;

	TWeakObjectPtr<UExplorationModeComponent> LeaderMode;

	// Stance being copied. Applied once the delay runs out.
	bool bTargetCrouched = false;

	bool bTargetSprinting = false;

	float StanceDelayLeft = 0.0f;
};
