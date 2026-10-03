// Copyright Xyzto Works


#include "Characters/AnimInstances/ExplorationEnemyAnimInstance.h"

#include "Nevergone.h"
#include "Characters/ExplorationEnemyCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"

void UExplorationEnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwningEnemy = Cast<AExplorationEnemyCharacter>(TryGetPawnOwner());

	if (!OwningEnemy.IsValid())
	{
		// Expected inside the AnimBP preview window, where there is no pawn.
		// A warning in PIE means the AnimBP was assigned to the wrong class.
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemyAnimInstance] %s has no AExplorationEnemyCharacter owner"),
			*GetNameSafe(this));
	}
}

void UExplorationEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	AExplorationEnemyCharacter* Enemy = OwningEnemy.Get();

	if (!Enemy)
	{
		return;
	}

	GroundSpeed = Enemy->GetVelocity().Size2D();

	// Three units per second is below anything a walk cycle should play for,
	// and above the residual drift a stopped character keeps for a few frames.
	bIsMoving = GroundSpeed > 3.0f;

	if (const UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
	{
		bIsFalling = Movement->IsFalling();
	}

	BehaviorState = Enemy->GetBehaviorState();

	bIsAlerted =
		BehaviorState == EExplorationEnemyState::Chase ||
		BehaviorState == EExplorationEnemyState::Catch;

	bIsCatching = BehaviorState == EExplorationEnemyState::Catch;

	bIsKnockedDown = BehaviorState == EExplorationEnemyState::KnockedDown;
}