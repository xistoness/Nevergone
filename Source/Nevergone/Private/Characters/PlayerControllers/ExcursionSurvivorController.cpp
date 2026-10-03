// Copyright Xyzto Works

#include "Characters/PlayerControllers/ExcursionSurvivorController.h"

#include "Nevergone.h"
#include "ActorComponents/ExplorationModeComponent.h"
#include "Characters/ExcursionSurvivorNPC.h"

#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"

AExcursionSurvivorController::AExcursionSurvivorController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AExcursionSurvivorController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ControlledSurvivor = Cast<AExcursionSurvivorNPC>(InPawn);

	if (!ControlledSurvivor.IsValid())
	{
		UE_LOG(LogNevergone, Error,
			TEXT("[ExcursionSurvivorController] %s possessed %s, which is not an AExcursionSurvivorNPC"),
			*GetNameSafe(this), *GetNameSafe(InPawn));

		return;
	}

	SurvivorMode = InPawn->FindComponentByClass<UExplorationModeComponent>();

	if (!SurvivorMode.IsValid())
	{
		UE_LOG(LogNevergone, Error,
			TEXT("[ExcursionSurvivorController] %s has no UExplorationModeComponent; it will follow but never sprint"),
			*GetNameSafe(InPawn));
	}

	ControlledSurvivor->OnSurvivorStateChanged.AddUObject(this, &AExcursionSurvivorController::HandleSurvivorStateChanged);

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionSurvivorController] %s possessed"),
		*GetNameSafe(InPawn));
}

void AExcursionSurvivorController::OnUnPossess()
{
	if (ControlledSurvivor.IsValid())
	{
		ControlledSurvivor->OnSurvivorStateChanged.RemoveAll(this);
	}

	StopMovement();

	ControlledSurvivor = nullptr;
	SurvivorMode = nullptr;
	CachedLeader = nullptr;
	LeaderMode = nullptr;

	Super::OnUnPossess();
}

void AExcursionSurvivorController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!ControlledSurvivor.IsValid() ||
		ControlledSurvivor->GetSurvivorState() != EExcursionSurvivorState::Following)
	{
		return;
	}

	// Frozen by a player capture: no moves, no stance copies.
	if (CustomTimeDilation <= 0.0f)
	{
		return;
	}

	ACharacter* Leader = GetLeader();

	if (!Leader)
	{
		// No player pawn this frame, e.g. mid possession swap.
		StopMovement();

		return;
	}

	if (Leader != CachedLeader.Get())
	{
		RefreshLeader(Leader);
	}

	TickFollow(Leader);

	TickMimic(DeltaTime, Leader);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

void AExcursionSurvivorController::HandleSurvivorStateChanged(AExcursionSurvivorNPC* Survivor,
	EExcursionSurvivorState NewState)
{
	switch (NewState)
	{
	case EExcursionSurvivorState::Following:
		// Start from the stance the follower already has, so the first copy
		// of the leader's stance also waits for the reaction delay.
		bTargetCrouched = Survivor->bIsCrouched;
		bTargetSprinting = false;
		StanceDelayLeft = 0.0f;
		break;

	case EExcursionSurvivorState::Waiting:
		// Hold position in whatever stance the follower is in. A crouched
		// follower left behind a counter stays hidden behind it.
		StopMovement();
		ApplyStance(Survivor->bIsCrouched, false);
		StanceDelayLeft = 0.0f;
		break;

	case EExcursionSurvivorState::Dead:
		StopMovement();
		break;

	default:
		break;
	}
}

void AExcursionSurvivorController::RefreshLeader(ACharacter* Leader)
{
	// A running move targets the previous leader.
	if (CachedLeader.IsValid())
	{
		StopMovement();
	}

	CachedLeader = Leader;

	LeaderMode = Leader->FindComponentByClass<UExplorationModeComponent>();

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionSurvivorController] %s now follows %s"),
		*GetNameSafe(GetPawn()), *GetNameSafe(Leader));
}

ACharacter* AExcursionSurvivorController::GetLeader() const
{
	return Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

// ---------------------------------------------------------------------------
// Follow
// ---------------------------------------------------------------------------

void AExcursionSurvivorController::TickFollow(ACharacter* Leader)
{
	const APawn* Follower = GetPawn();

	if (!Follower)
	{
		return;
	}

	// A running move already tracks the leader: a move to an actor repaths
	// as its goal moves, and ends on its own at StopDistance.
	if (GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		return;
	}

	const float Distance = FVector::Dist(Follower->GetActorLocation(), Leader->GetActorLocation());

	if (Distance <= ResumeDistance)
	{
		return;
	}

	// bStopOnOverlap off: with it on, the acceptance radius grows by both
	// capsules and the follower would stop almost at ResumeDistance,
	// starting and stopping on every step.
	const EPathFollowingRequestResult::Type Result = MoveToActor(Leader, StopDistance, false);

	if (Result == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExcursionSurvivorController] %s could not path to %s (distance %.0f)"),
			*GetNameSafe(Follower), *GetNameSafe(Leader), Distance);
	}
}

// ---------------------------------------------------------------------------
// Mimic
// ---------------------------------------------------------------------------

void AExcursionSurvivorController::TickMimic(float DeltaTime, const ACharacter* Leader)
{
	const bool bLeaderCrouched = Leader->bIsCrouched;

	const bool bLeaderSprinting = LeaderMode.IsValid() && LeaderMode->IsSprinting();

	if (bLeaderCrouched != bTargetCrouched || bLeaderSprinting != bTargetSprinting)
	{
		bTargetCrouched = bLeaderCrouched;
		bTargetSprinting = bLeaderSprinting;

		StanceDelayLeft = MimicDelaySeconds;

		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExcursionSurvivorController] %s saw the leader change stance: crouched %d, sprinting %d"),
			*GetNameSafe(GetPawn()), bTargetCrouched ? 1 : 0, bTargetSprinting ? 1 : 0);
	}

	// Until the delay runs out the follower keeps the previous stance.
	if (StanceDelayLeft > 0.0f)
	{
		StanceDelayLeft -= DeltaTime;

		return;
	}

	ApplyStance(bTargetCrouched, bTargetSprinting);
}

void AExcursionSurvivorController::ApplyStance(bool bCrouched, bool bSprinting)
{
	AExcursionSurvivorNPC* Survivor = ControlledSurvivor.Get();

	if (!Survivor)
	{
		return;
	}

	if (bCrouched && !Survivor->bIsCrouched)
	{
		Survivor->Crouch();
	}
	else if (!bCrouched && Survivor->bIsCrouched)
	{
		Survivor->UnCrouch();
	}

	// Sprint goes through the same door as player input, so the follower's
	// own stamina decides whether it actually sprints, and runs out and
	// pants on its own.
	if (UExplorationModeComponent* Mode = SurvivorMode.Get())
	{
		if (bSprinting)
		{
			Mode->Input_SprintPressed();
		}
		else
		{
			Mode->Input_SprintReleased();
		}
	}
}
