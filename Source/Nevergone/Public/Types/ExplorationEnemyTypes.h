// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "ExplorationEnemyTypes.generated.h"

class UAnimInstance;
class UAnimMontage;
class USkeletalMesh;

// ---------------------------------------------------------------------------
// EExplorationPatrolMode
//
// How an enemy type occupies the level while it has not noticed anything.
// Chosen per enemy type in UExplorationEnemyDefinition.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EExplorationPatrolMode : uint8
{
	// Holds its spawn point and only rotates to scan. Used for corridor watchers.
	Stationary,

	// Walks the ordered PatrolWaypoints list authored on the placed actor.
	// Falls back to Stationary when the list has fewer than two entries.
	Route,

	// Picks random navigable points inside WanderRadius around its spawn point.
	Wander,
};

// ---------------------------------------------------------------------------
// EExplorationEnemyState
//
// Behaviour state owned by the character and driven by AExplorationEnemyController.
// Kept on the character (not the controller) so the AnimInstance can read it
// without knowing anything about the AI.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EExplorationEnemyState : uint8
{
	// Spawned but not acting yet. Only used before the controller takes over.
	Idle,

	// Executing the patrol mode of its definition. Nothing noticed.
	Patrol,

	// Something was sensed but not confirmed. Moves to the stimulus location.
	Investigate,

	// Target confirmed. Moves toward it at ChaseSpeed.
	Chase,

	// Target is within CatchDistance and UExcursionEncounterSubsystem accepted
	// the capture. The paired animation drives the body from here.
	Catch,

	// Pushed to the ground by a bitten survivor. Gets up after the type's
	// KnockdownSeconds and chases the survivor again.
	KnockedDown,

	// Killed by a survivor's safeguard. The body is a ragdoll and the brain
	// never acts again.
	Dead,
};

// ---------------------------------------------------------------------------
// FExplorationEnemyVisualVariant
//
// One art option for an enemy type. A type lists several and each spawned
// actor resolves exactly one, so a corridor of "walkers" does not look cloned.
//
// References are soft on purpose: the definition asset is loaded as soon as
// any actor of the type exists, and hard references would drag every variant
// mesh and animation blueprint into memory alongside it.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FExplorationEnemyVisualVariant
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	// Animation blueprint for this mesh. Variants of the same type rarely share
	// a skeleton proportion, so the anim class is paired with the mesh instead
	// of being a single per-type value.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TSoftClassPtr<UAnimInstance> AnimClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", meta = (ClampMin = "0.1"))
	float UniformScale = 1.0f;

	bool IsUsable() const
	{
		return !Mesh.IsNull();
	}
};

// ---------------------------------------------------------------------------
// FExcursionCaptureAnimation
//
// One paired animation of a capture: what the enemy plays and what the
// victim plays, authored together and started on the same frame by
// UExcursionEncounterSubsystem, with the enemy placed in front of the
// victim at the type's CaptureStandOffDistance.
//
// Either montage may be empty. With both empty the capture resolves at once,
// which is how the flow works before the art exists.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FExcursionCaptureAnimation
{
	GENERATED_BODY()

	// Must match the skeleton of every visual variant of the type.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture")
	TObjectPtr<UAnimMontage> EnemyMontage = nullptr;

	// Must match the survivors' skeleton.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture")
	TObjectPtr<UAnimMontage> VictimMontage = nullptr;
};