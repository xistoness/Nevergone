// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Types/ExplorationEnemyTypes.h"
#include "ExplorationEnemyAnimInstance.generated.h"

class AExplorationEnemyCharacter;

// ---------------------------------------------------------------------------
// UExplorationEnemyAnimInstance
//
// Base class for every exploration enemy AnimBP. All the thinking happens
// here; the Blueprint that derives from it holds only the state machine, the
// blend space and the asset references.
//
// Every AnimBP assigned to a FExplorationEnemyVisualVariant must be reparented
// to this class, otherwise the state machine has no variables to read and the
// enemy T-poses.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UExplorationEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	virtual void NativeInitializeAnimation() override;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:

	// Horizontal speed in cm/s. Drives the locomotion blend space directly:
	// because MaxWalkSpeed swaps between PatrolSpeed and ChaseSpeed with the
	// behaviour state, a single 1D blend space covers idle, shamble and run
	// without any transition logic.
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float GroundSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsMoving = false;

	// Whether the enemy is airborne. Zombies do not jump, but ledges and
	// ragdoll recovery exist, and a falling enemy playing a walk cycle reads
	// as a bug.
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsFalling = false;

	// Raw behaviour state, exposed for state machine transitions and for any
	// per-state additive layer (a head turn while scanning, for instance).
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	EExplorationEnemyState BehaviorState = EExplorationEnemyState::Idle;

	// True in Chase and Catch. Use it to pick an aggressive upper-body pose
	// rather than to pick a locomotion speed, which GroundSpeed already covers.
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	bool bIsAlerted = false;

	// True while a capture plays. The paired montage drives the body; use
	// this only for what the montage does not cover.
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	bool bIsCatching = false;

	// True while the enemy lies on the ground after a bitten survivor pushed
	// it away. Drives the down loop and the get-up transition.
	UPROPERTY(BlueprintReadOnly, Category = "Behaviour")
	bool bIsKnockedDown = false;

private:

	TWeakObjectPtr<AExplorationEnemyCharacter> OwningEnemy;
};