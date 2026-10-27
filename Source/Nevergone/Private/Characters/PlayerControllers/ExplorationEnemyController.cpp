// Copyright Xyzto Works


#include "Characters/PlayerControllers/ExplorationEnemyController.h"

#include "Nevergone.h"
#include "ActorComponents/ExplorationPerceptionComponent.h"
#include "Actors/Door.h"
#include "Characters/ExcursionSurvivorNPC.h"
#include "Characters/ExplorationEnemyCharacter.h"
#include "Data/ExplorationEnemyDefinition.h"
#include "EngineTools/AINavLinks.h"
#include "EngineTools/CollisionChannels.h"
#include "World/ExcursionEncounterSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

namespace ExplorationEnemyAI
{
	// A place noise this close to a point the enemy is already at (or
	// already investigating) points at the same place.
	static constexpr float SamePlaceRadius = 100.0f;

	// Acceptance of a chase move while a closed door or a wall stands
	// between the enemy and its target: well under any distance across a
	// door or a wall, and the catch happens at CatchDistance long before it
	// is reached.
	static constexpr float LineBlockedChaseAcceptanceRadius = 10.0f;

	// Hits on the capture line that are not scenery (a closed door's box, a
	// ragdoll) skipped before the line is taken as free.
	static constexpr int32 MaxCaptureLineSkippedHits = 8;
}

AExplorationEnemyController::AExplorationEnemyController()
{
	PrimaryActorTick.bCanEverTick = true;

	// AAIController already declares PerceptionComponent. This is the typed
	// handle to the same object, registered through SetPerceptionComponent;
	// always go through EnemyPerceptionComponent in this class.
	EnemyPerceptionComponent = CreateDefaultSubobject<UExplorationPerceptionComponent>(TEXT("ExplorationPerception"));

	SetPerceptionComponent(*EnemyPerceptionComponent);
}

void AExplorationEnemyController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ControlledEnemy = Cast<AExplorationEnemyCharacter>(InPawn);

	if (!ControlledEnemy.IsValid())
	{
		UE_LOG(LogNevergone, Error,
			TEXT("[ExplorationEnemyController] %s possessed %s, which is not an AExplorationEnemyCharacter"),
			*GetNameSafe(this), *GetNameSafe(InPawn));

		return;
	}

	Definition = ControlledEnemy->GetDefinition();

	if (!Definition)
	{
		UE_LOG(LogNevergone, Error,
			TEXT("[ExplorationEnemyController] %s has no definition on %s; staying idle"),
			*GetNameSafe(this), *GetNameSafe(InPawn));

		return;
	}

	HomeRotation = InPawn->GetActorRotation();

	if (EnemyPerceptionComponent)
	{
		EnemyPerceptionComponent->InitializeFromDefinition(Definition);

		EnemyPerceptionComponent->OnSurvivorSensed.AddUObject(this, &AExplorationEnemyController::HandleSurvivorSensed);
		EnemyPerceptionComponent->OnSurvivorLost.AddUObject(this, &AExplorationEnemyController::HandleSurvivorLost);
		EnemyPerceptionComponent->OnPlaceNoiseHeard.AddUObject(this, &AExplorationEnemyController::HandlePlaceNoiseHeard);
	}

	// Placed pawns are possessed inside PostInitializeComponents, before any
	// BeginPlay: the pawn's home location is not set yet. Patrol starts on the
	// first tick instead.
	bPatrolStartPending = true;
}

void AExplorationEnemyController::OnUnPossess()
{
	if (EnemyPerceptionComponent)
	{
		EnemyPerceptionComponent->OnSurvivorSensed.RemoveAll(this);
		EnemyPerceptionComponent->OnSurvivorLost.RemoveAll(this);
		EnemyPerceptionComponent->OnPlaceNoiseHeard.RemoveAll(this);
	}

	ControlledEnemy = nullptr;
	Definition = nullptr;
	ChaseTarget = nullptr;
	ConfirmTarget = nullptr;
	Suspicion.Reset();
	ForgetDoorKnock();

	bPatrolStartPending = false;
	bStateStartAfterDoorway = false;
	DoorwayCrossing.Reset();
	CatchBlock = ECaptureLineBlock::None;
	CatchBlocker = nullptr;

	Super::OnUnPossess();
}

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

void AExplorationEnemyController::EnterState(EExplorationEnemyState NewState)
{
	if (!ControlledEnemy.IsValid())
	{
		return;
	}

	if (ControlledEnemy->GetBehaviorState() == NewState)
	{
		return;
	}

	// The character owns the state so the AnimInstance can read it without
	// knowing the controller exists.
	ControlledEnemy->SetBehaviorState(NewState);

	// A start still waiting for the doorway belonged to the previous state.
	bStateStartAfterDoorway = false;

	switch (NewState)
	{
	case EExplorationEnemyState::Patrol:
		Suspicion.Reset();
		TimeSinceLastContact = 0.0f;
		ForgetDoorKnock();

		// Giving up halfway through a door: across first, then back to the
		// post or the route.
		if (!DeferStateStartToDoorwayExit())
		{
			StartPatrol();
		}
		break;

	case EExplorationEnemyState::Suspicious:
		// Stand still: the turn towards the survivor is the whole beat.
		if (!DeferStateStartToDoorwayExit())
		{
			StopMovement();
		}
		break;

	case EExplorationEnemyState::Investigate:
		BeginInvestigation();
		break;

	case EExplorationEnemyState::Chase:
		TimeSinceLastContact = 0.0f;
		TimeSinceRepath = ChaseRepathInterval;
		CatchBlock = ECaptureLineBlock::None;
		CatchBlocker = nullptr;
		break;

	case EExplorationEnemyState::Catch:
	case EExplorationEnemyState::KnockedDown:
		// The capture places the body, wherever it stood.
		DoorwayCrossing.Reset();
		StopMovement();
		break;

	case EExplorationEnemyState::Dead:
		DoorwayCrossing.Reset();
		StopMovement();
		ChaseTarget = nullptr;
		ConfirmTarget = nullptr;
		ForgetDoorKnock();

		if (EnemyPerceptionComponent)
		{
			// Smell is the only sense that polls; the others only matter
			// through handlers, which ignore a dead enemy.
			EnemyPerceptionComponent->SetComponentTickEnabled(false);
		}
		break;

	default:
		break;
	}
}

void AExplorationEnemyController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!ControlledEnemy.IsValid() || !Definition)
	{
		return;
	}

	// Frozen by a player capture. Delta time is zero, but a zero-time tick
	// could still report a catch or issue moves; nothing may run.
	if (CustomTimeDilation <= 0.0f)
	{
		return;
	}

	if (bPatrolStartPending)
	{
		bPatrolStartPending = false;

		EnterState(EExplorationEnemyState::Patrol);

		return;
	}

	switch (ControlledEnemy->GetBehaviorState())
	{
	case EExplorationEnemyState::Dead:
	case EExplorationEnemyState::Catch:
		// Dead never acts again; Catch waits for the capture sequence.
		return;

	case EExplorationEnemyState::KnockedDown:
		TickKnockdown(DeltaTime);
		return;

	default:
		break;
	}

	// A move that ended in a doorway walks out of it.
	DoorwayCrossing.Tick(*this);

	// Sight escalation is handled here rather than in the sensed handler, so a
	// survivor standing still in view still accumulates contact time: the
	// perception system stops firing events once the stimulus is stable.
	//
	// The survivor being confirmed is kept while it stays in view. Picking
	// the closest one every frame would restart the window each time two
	// survivors trade places, and the confirmation would never complete.
	// Once it is lost, a suspicious enemy moves to the other survivor in view
	// without resetting the confirmation: it belongs to the enemy.
	const EExplorationEnemyState StateBeforeSight = ControlledEnemy->GetBehaviorState();

	const bool bWasSuspicious = StateBeforeSight == EExplorationEnemyState::Suspicious;

	AActor* Focus = ConfirmTarget.Get();

	if (EnemyPerceptionComponent && (!Focus || !EnemyPerceptionComponent->HasSightOf(Focus)))
	{
		AActor* Sighted = EnemyPerceptionComponent->GetClosestSightedSurvivor();

		if (bWasSuspicious && Focus && !Sighted)
		{
			// A suspicious enemy keeps its target through a short loss of
			// sight. TickSuspicious decides when the loss is too long.
			Focus = nullptr;
		}
		else
		{
			if (Sighted != ConfirmTarget.Get())
			{
				if (bWasSuspicious && Sighted)
				{
					UE_LOG(LogNevergone, Verbose,
						TEXT("[ExplorationEnemyController] %s lost sight of %s and switches to %s; confirmation kept at %.2fs"),
						*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(ConfirmTarget.Get()), *GetNameSafe(Sighted),
						Suspicion.GetContactSeconds());

					Suspicion.SwitchTarget(Sighted->GetActorLocation());
				}
				else
				{
					Suspicion.RestartContact();
				}

				ConfirmTarget = Sighted;
			}

			Focus = Sighted;
		}
	}

	if (bWasSuspicious)
	{
		// The open window advances in TickSuspicious, which also confirms.
	}
	else if (Focus)
	{
		Suspicion.AddContact(DeltaTime);

		if (Suspicion.GetContactSeconds() >= Definition->TimeToConfirmSeconds &&
			StateBeforeSight != EExplorationEnemyState::Chase &&
			StateBeforeSight != EExplorationEnemyState::Catch)
		{
			ChaseConfirmedTarget(Focus);
		}
		else if (StateBeforeSight == EExplorationEnemyState::Idle ||
			StateBeforeSight == EExplorationEnemyState::Patrol ||
			StateBeforeSight == EExplorationEnemyState::Investigate)
		{
			// The confirmation window is open: stop and look. Decided here
			// rather than in the sight handler for the same reason as the
			// escalation: a survivor recruited while already in view fires
			// no new sight event.
			UE_LOG(LogNevergone, Log,
				TEXT("[ExplorationEnemyController] %s is suspicious of %s"),
				*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Focus));

			Suspicion.Open(Focus->GetActorLocation());

			EnterState(EExplorationEnemyState::Suspicious);
		}
	}
	else
	{
		Suspicion.Reset();
	}

	// A state entered halfway through a door: it already changed, the body
	// finishes crossing first.
	if (bStateStartAfterDoorway)
	{
		if (DoorwayCrossing.IsCrossing(*this))
		{
			return;
		}

		bStateStartAfterDoorway = false;

		StartStateAfterDoorway();
	}

	TickDoorKnock(DeltaTime);

	switch (ControlledEnemy->GetBehaviorState())
	{
	case EExplorationEnemyState::Idle:
	case EExplorationEnemyState::Patrol:
		TickPatrol(DeltaTime);
		break;

	case EExplorationEnemyState::Suspicious:
		// A window opened this tick already counted this tick's contact.
		if (bWasSuspicious)
		{
			TickSuspicious(DeltaTime, Focus);
		}
		break;

	case EExplorationEnemyState::Investigate:
		TickInvestigate(DeltaTime);
		break;

	case EExplorationEnemyState::Chase:
		TickChase(DeltaTime);
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Patrol
// ---------------------------------------------------------------------------

void AExplorationEnemyController::StartPatrol()
{
	CurrentWaypointIndex = 0;
	WaitRemaining = 0.0f;
	ScanOffsetDegrees = 0.0f;
	ScanDirection = 1.0f;
	bReturningToPost = false;
	bRestoringHomeRotation = false;

	if (!ControlledEnemy.IsValid())
	{
		return;
	}

	switch (ControlledEnemy->GetEffectivePatrolMode())
	{
	case EExplorationPatrolMode::Route:
		AdvanceRoute();
		break;

	case EExplorationPatrolMode::Wander:
		PickWanderDestination();
		break;

	case EExplorationPatrolMode::Stationary:
	default:
	{
		StopMovement();

		// Back to the post it was placed at, facing the way it was placed:
		// a guard that gave up resumes guarding the same spot. On the first
		// patrol it is already there and both steps finish at once.
		bRestoringHomeRotation = true;

		const float DistanceFromPost =
			FVector::Dist2D(ControlledEnemy->GetActorLocation(), ControlledEnemy->GetHomeLocation());

		if (DistanceFromPost > MoveAcceptanceRadius)
		{
			if (MoveToLocation(ControlledEnemy->GetHomeLocation(), MoveAcceptanceRadius) == EPathFollowingRequestResult::Failed)
			{
				UE_LOG(LogNevergone, Warning,
					TEXT("[ExplorationEnemyController] %s could not path back to its post; guarding from where it stands"),
					*GetNameSafe(ControlledEnemy.Get()));
			}
			else
			{
				bReturningToPost = true;

				UE_LOG(LogNevergone, Verbose,
					TEXT("[ExplorationEnemyController] %s is returning to its post (%.0f away)"),
					*GetNameSafe(ControlledEnemy.Get()), DistanceFromPost);
			}
		}
		break;
	}
	}
}

void AExplorationEnemyController::TickPatrol(float DeltaTime)
{
	if (!ControlledEnemy.IsValid())
	{
		return;
	}

	const EExplorationPatrolMode Mode = ControlledEnemy->GetEffectivePatrolMode();

	if (Mode == EExplorationPatrolMode::Stationary)
	{
		TickStationaryScan(DeltaTime);

		return;
	}

	if (WaitRemaining > 0.0f)
	{
		WaitRemaining -= DeltaTime;

		return;
	}

	if (!HasReachedDestination())
	{
		return;
	}

	WaitRemaining = Definition ? Definition->WaitAtDestinationSeconds : 0.0f;

	if (Mode == EExplorationPatrolMode::Route)
	{
		AdvanceRoute();
	}
	else
	{
		PickWanderDestination();
	}
}

bool AExplorationEnemyController::UpdateReturnToPost(float DeltaTime)
{
	if (bReturningToPost)
	{
		if (!HasReachedDestination())
		{
			return false;
		}

		bReturningToPost = false;
	}

	if (bRestoringHomeRotation)
	{
		// A tight tolerance: the scan sweeps around HomeRotation and snaps
		// the rest of the way on its first frame, so the gap must not show.
		if (!TurnTowardsYaw(HomeRotation.Yaw, 0.5f, DeltaTime))
		{
			return false;
		}

		bRestoringHomeRotation = false;
	}

	return true;
}

void AExplorationEnemyController::TickStationaryScan(float DeltaTime)
{
	if (!Definition || !ControlledEnemy.IsValid())
	{
		return;
	}

	if (!UpdateReturnToPost(DeltaTime) || !Definition->bScanWhileStationary)
	{
		return;
	}

	if (WaitRemaining > 0.0f)
	{
		WaitRemaining -= DeltaTime;

		return;
	}

	const float Step = Definition->TurnRateDegrees * DeltaTime * ScanDirection;

	ScanOffsetDegrees += Step;

	// Pause at each end of the sweep. The pause is what gives the player a
	// readable rhythm to move against.
	if (FMath::Abs(ScanOffsetDegrees) >= Definition->ScanHalfAngleDegrees)
	{
		ScanOffsetDegrees = FMath::Clamp(ScanOffsetDegrees,
			-Definition->ScanHalfAngleDegrees, Definition->ScanHalfAngleDegrees);

		ScanDirection *= -1.0f;

		WaitRemaining = Definition->WaitAtDestinationSeconds;
	}

	FRotator ScanRotation = HomeRotation;

	ScanRotation.Yaw += ScanOffsetDegrees;

	ControlledEnemy->SetActorRotation(ScanRotation);
}

void AExplorationEnemyController::AdvanceRoute()
{
	if (!ControlledEnemy.IsValid())
	{
		return;
	}

	const TArray<TObjectPtr<AActor>>& Waypoints = ControlledEnemy->GetPatrolWaypoints();

	if (Waypoints.Num() == 0)
	{
		return;
	}

	// Skip destroyed waypoints instead of stalling on them.
	for (int32 Attempt = 0; Attempt < Waypoints.Num(); ++Attempt)
	{
		CurrentWaypointIndex = (CurrentWaypointIndex + 1) % Waypoints.Num();

		if (IsValid(Waypoints[CurrentWaypointIndex]))
		{
			const FVector Destination = Waypoints[CurrentWaypointIndex]->GetActorLocation();

			const EPathFollowingRequestResult::Type Result =
				MoveToLocation(Destination, MoveAcceptanceRadius);

			if (Result == EPathFollowingRequestResult::Failed)
			{
				UE_LOG(LogNevergone, Warning,
					TEXT("[ExplorationEnemyController] %s could not path to waypoint %d. Is the NavMeshBoundsVolume covering it?"),
					*GetNameSafe(ControlledEnemy.Get()), CurrentWaypointIndex);
			}

			return;
		}
	}
}

void AExplorationEnemyController::PickWanderDestination()
{
	if (!ControlledEnemy.IsValid() || !Definition)
	{
		return;
	}

	if (!MoveToRandomReachablePoint(ControlledEnemy->GetHomeLocation(), Definition->WanderRadius))
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExplorationEnemyController] %s found no reachable wander point within %.0f of home. Is there a NavMesh here?"),
			*GetNameSafe(ControlledEnemy.Get()), Definition->WanderRadius);

		// Retry on the next pause instead of hammering the navigation system.
		WaitRemaining = 1.0f;
	}
}

bool AExplorationEnemyController::MoveToRandomReachablePoint(const FVector& Origin, float Radius)
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	if (!NavSystem)
	{
		return false;
	}

	FNavLocation Destination;

	if (!NavSystem->GetRandomReachablePointInRadius(Origin, Radius, Destination))
	{
		return false;
	}

	return MoveToLocation(Destination.Location, MoveAcceptanceRadius) != EPathFollowingRequestResult::Failed;
}

bool AExplorationEnemyController::HasReachedDestination() const
{
	const UPathFollowingComponent* PathFollowing = GetPathFollowingComponent();

	if (!PathFollowing)
	{
		return true;
	}

	return PathFollowing->GetStatus() == EPathFollowingStatus::Idle;
}

bool AExplorationEnemyController::TurnTowardsYaw(float TargetYaw, float ToleranceDegrees, float DeltaTime)
{
	AExplorationEnemyCharacter* Enemy = ControlledEnemy.Get();

	if (!Enemy || !Definition)
	{
		return true;
	}

	const FRotator CurrentRotation = Enemy->GetActorRotation();

	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetYaw);

	if (FMath::Abs(DeltaYaw) <= ToleranceDegrees)
	{
		return true;
	}

	// Same turn rate everywhere, so an enemy turns at one speed.
	const float Step = FMath::Min(Definition->TurnRateDegrees * DeltaTime, FMath::Abs(DeltaYaw));

	FRotator NewRotation = CurrentRotation;
	NewRotation.Yaw += FMath::Sign(DeltaYaw) * Step;

	Enemy->SetActorRotation(NewRotation);

	return false;
}

// ---------------------------------------------------------------------------
// Suspicious
// ---------------------------------------------------------------------------

void AExplorationEnemyController::TickSuspicious(float DeltaTime, AActor* Focus)
{
	AExplorationEnemyCharacter* Enemy = ControlledEnemy.Get();

	if (!Enemy || !Definition)
	{
		return;
	}

	const FVector FocusLocation = Focus ? Focus->GetActorLocation() : FVector::ZeroVector;

	const bool bHadHeldNoise = Suspicion.HasHeldNoise();

	const EExplorationSuspicionStep Step = Suspicion.Advance(Focus != nullptr, FocusLocation,
		DeltaTime, Definition->SuspicionSightLossToleranceSeconds);

	switch (Step)
	{
	case EExplorationSuspicionStep::LostSight:
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemyController] %s lost sight of %s while suspicious; tolerating up to %.2fs (confirmation paused at %.2fs)"),
			*GetNameSafe(Enemy), *GetNameSafe(ConfirmTarget.Get()),
			Definition->SuspicionSightLossToleranceSeconds, Suspicion.GetContactSeconds());
		break;

	case EExplorationSuspicionStep::Regained:
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemyController] %s saw %s again within the tolerance; confirmation resumes at %.2fs%s"),
			*GetNameSafe(Enemy), *GetNameSafe(Focus), Suspicion.GetContactSeconds(),
			bHadHeldNoise ? TEXT(", the held noise is superseded") : TEXT(""));
		break;

	case EExplorationSuspicionStep::Expired:
	{
		// The window closed without a confirmation. Investigate goes to the
		// most recent stimulus, made current here.
		const bool bUsesHeldNoise = Suspicion.HasHeldNoise();

		const FVector Stimulus = Suspicion.ResolveStimulus();

		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemyController] %s left the tolerance: %s out of view for %.2fs"),
			*GetNameSafe(Enemy), *GetNameSafe(ConfirmTarget.Get()), Suspicion.GetSightLossSeconds());

		if (bUsesHeldNoise)
		{
			UE_LOG(LogNevergone, Verbose,
				TEXT("[ExplorationEnemyController] %s uses the held noise at %s"),
				*GetNameSafe(Enemy), *Stimulus.ToCompactString());
		}

		UE_LOG(LogNevergone, Log,
			TEXT("[ExplorationEnemyController] %s lost sight before confirming and is investigating the %s"),
			*GetNameSafe(Enemy), bUsesHeldNoise ? TEXT("held noise") : TEXT("last seen point"));

		if (EnemyPerceptionComponent)
		{
			EnemyPerceptionComponent->SetMostRecentStimulus(Stimulus);
		}

		ConfirmTarget = nullptr;

		Suspicion.Reset();

		EnterState(EExplorationEnemyState::Investigate);

		return;
	}

	default:
		break;
	}

	if (Focus && Suspicion.GetContactSeconds() >= Definition->TimeToConfirmSeconds)
	{
		if (Suspicion.HasHeldNoise())
		{
			UE_LOG(LogNevergone, Verbose,
				TEXT("[ExplorationEnemyController] %s discards the held noise at %s"),
				*GetNameSafe(Enemy), *Suspicion.GetHeldNoiseLocation().ToCompactString());

			Suspicion.DiscardHeldNoise();
		}

		ChaseConfirmedTarget(Focus);

		return;
	}

	// Keeps tracking a survivor who moves while being watched, and faces the
	// last seen point while they are out of view.
	FVector ToLook = (Focus ? FocusLocation : Suspicion.GetLastSeenLocation()) - Enemy->GetActorLocation();
	ToLook.Z = 0.0f;

	if (!ToLook.IsNearlyZero())
	{
		TurnTowardsYaw(ToLook.Rotation().Yaw, 1.0f, DeltaTime);
	}
}

void AExplorationEnemyController::ChaseConfirmedTarget(AActor* Target)
{
	ChaseTarget = Target;

	UE_LOG(LogNevergone, Log,
		TEXT("[ExplorationEnemyController] %s confirmed %s and is chasing"),
		*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Target));

	EnterState(EExplorationEnemyState::Chase);
}

// ---------------------------------------------------------------------------
// Investigate
//
// Two phases. Travel: walk to the last known location. Search: wander between
// random reachable points around it, pausing at each, until the search budget
// runs out. The budget starts on arrival, so a far-away noise does not eat
// the whole search in walking time.
// ---------------------------------------------------------------------------

void AExplorationEnemyController::BeginInvestigation()
{
	InvestigateElapsed = 0.0f;
	WaitRemaining = 0.0f;
	bInvestigateMoveIssued = false;
	bIsSearching = false;

	if (!DeferStateStartToDoorwayExit())
	{
		StopMovement();
	}

	// Face the stimulus before travelling to it. The eyes follow the body,
	// so turning in place is what lets the enemy spot a survivor who is
	// across the room rather than at the end of the path.
	bFacingStimulus = false;
	FaceStimulusElapsed = 0.0f;

	if (Definition && Definition->bFaceStimulusBeforeMoving &&
		EnemyPerceptionComponent && EnemyPerceptionComponent->HasLastKnownLocation())
	{
		FaceTarget = EnemyPerceptionComponent->GetLastKnownLocation();

		bFacingStimulus = true;
	}
}

bool AExplorationEnemyController::UpdateStimulusFacing(float DeltaTime)
{
	AExplorationEnemyCharacter* Enemy = ControlledEnemy.Get();

	if (!Enemy || !Definition)
	{
		bFacingStimulus = false;

		return true;
	}

	FaceStimulusElapsed += DeltaTime;

	FVector ToTarget = FaceTarget - Enemy->GetActorLocation();
	ToTarget.Z = 0.0f;

	const bool bOutOfTime = FaceStimulusElapsed >= Definition->FaceStimulusMaxSeconds;

	if (ToTarget.IsNearlyZero() || bOutOfTime)
	{
		bFacingStimulus = false;

		return true;
	}

	if (TurnTowardsYaw(ToTarget.Rotation().Yaw, Definition->FaceStimulusToleranceDegrees, DeltaTime))
	{
		bFacingStimulus = false;

		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemyController] %s is facing the stimulus after %.2fs"),
			*GetNameSafe(Enemy), FaceStimulusElapsed);

		return true;
	}

	return false;
}

void AExplorationEnemyController::TickInvestigate(float DeltaTime)
{
	if (!Definition || !EnemyPerceptionComponent || !ControlledEnemy.IsValid())
	{
		return;
	}

	// --- Facing ---

	// Nothing else runs until the enemy has turned: it is a beat of standing
	// still, and it is also the window in which it can see the survivor from
	// where it already is.
	if (bFacingStimulus && !UpdateStimulusFacing(DeltaTime))
	{
		return;
	}

	// --- Travel ---

	if (!bIsSearching)
	{
		if (!bInvestigateMoveIssued)
		{
			bInvestigateMoveIssued = true;

			SearchAnchor = EnemyPerceptionComponent->HasLastKnownLocation()
				? EnemyPerceptionComponent->GetLastKnownLocation()
				: ControlledEnemy->GetActorLocation();

			if (MoveToLocation(SearchAnchor, MoveAcceptanceRadius) == EPathFollowingRequestResult::Failed)
			{
				UE_LOG(LogNevergone, Warning,
					TEXT("[ExplorationEnemyController] %s could not path to the stimulus location; searching from where it stands"),
					*GetNameSafe(ControlledEnemy.Get()));

				SearchAnchor = ControlledEnemy->GetActorLocation();
			}

			return;
		}

		if (!HasReachedDestination())
		{
			return;
		}

		bIsSearching = true;

		// Stop at the spot first. This pause is the "looking around" beat.
		WaitRemaining = Definition->WaitAtDestinationSeconds;

		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemyController] %s reached the last known location and is searching"),
			*GetNameSafe(ControlledEnemy.Get()));
	}

	// --- Search ---

	// Knocking at a door on the way to a search point is not searching.
	if (!IsKnocking())
	{
		InvestigateElapsed += DeltaTime;
	}

	if (InvestigateElapsed >= Definition->InvestigateDurationSeconds)
	{
		EnemyPerceptionComponent->ForgetLastKnownLocation();

		UE_LOG(LogNevergone, Log,
			TEXT("[ExplorationEnemyController] %s gave up searching and is returning to patrol"),
			*GetNameSafe(ControlledEnemy.Get()));

		EnterState(EExplorationEnemyState::Patrol);

		return;
	}

	if (!HasReachedDestination())
	{
		return;
	}

	if (WaitRemaining > 0.0f)
	{
		WaitRemaining -= DeltaTime;

		return;
	}

	// Armed before the move and only consumed after the next arrival, so the
	// enemy stops briefly at every point it checks.
	WaitRemaining = Definition->WaitAtDestinationSeconds;

	MoveToRandomReachablePoint(SearchAnchor, Definition->SearchRadius);
}

// ---------------------------------------------------------------------------
// Chase
// ---------------------------------------------------------------------------

void AExplorationEnemyController::TickChase(float DeltaTime)
{
	if (!ControlledEnemy.IsValid() || !Definition)
	{
		return;
	}

	AActor* Target = ChaseTarget.Get();

	// A target stops counting when it is destroyed or is no longer a body
	// enemies hunt (the party leader was swapped away from it).
	if (!IsValid(Target) || !AExcursionSurvivorNPC::IsSurvivorBody(Target))
	{
		UE_LOG(LogNevergone, Log,
			TEXT("[ExplorationEnemyController] %s has no valid chase target (%s); returning to patrol"),
			*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Target));

		EnterState(EExplorationEnemyState::Patrol);

		return;
	}

	// A closed door or a wall between them holds the capture, however close
	// they stand. It also changes the chase move below.
	const AActor* LineBlocker = nullptr;

	const ECaptureLineBlock LineBlock = FindCaptureLineBlock(Target, LineBlocker);

	const bool bWithinCatchDistance = ControlledEnemy->IsWithinCatchDistance(Target);

	const ECaptureLineBlock HeldBlock = bWithinCatchDistance ? LineBlock : ECaptureLineBlock::None;

	const AActor* HeldBlocker = bWithinCatchDistance ? LineBlocker : nullptr;

	if (HeldBlock != CatchBlock || HeldBlocker != CatchBlocker.Get())
	{
		if (HeldBlock == ECaptureLineBlock::ClosedDoor)
		{
			const ADoor* Door = Cast<ADoor>(HeldBlocker);

			UE_LOG(LogNevergone, Verbose,
				TEXT("[ExplorationEnemyController] %s cannot catch %s through %s (%s); it goes through the door first"),
				*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Target), *GetNameSafe(Door),
				Door ? *UEnum::GetValueAsString(Door->GetState()) : TEXT("?"));
		}
		else if (HeldBlock == ECaptureLineBlock::Scenery)
		{
			UE_LOG(LogNevergone, Verbose,
				TEXT("[ExplorationEnemyController] %s cannot catch %s through %s (scenery); it goes around"),
				*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Target), *GetNameSafe(HeldBlocker));
		}

		CatchBlock = HeldBlock;
		CatchBlocker = HeldBlocker;
	}

	if (CaptureRules::CanCatch(bWithinCatchDistance, LineBlock))
	{
		UExcursionEncounterSubsystem* Encounter = UExcursionEncounterSubsystem::Get(this);

		ACharacter* Victim = Cast<ACharacter>(Target);

		if (Encounter && Victim && Encounter->ReportCatch(ControlledEnemy.Get(), Victim))
		{
			UE_LOG(LogNevergone, Log,
				TEXT("[ExplorationEnemyController] %s caught %s"),
				*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Target));

			EnterState(EExplorationEnemyState::Catch);
		}

		// Refused (the victim is already in a capture): stay on it and try
		// again next frame.
		return;
	}

	const bool bHasContact = EnemyPerceptionComponent && EnemyPerceptionComponent->HasContactWith(Target);

	if (bHasContact)
	{
		TimeSinceLastContact = 0.0f;
	}
	else
	{
		TimeSinceLastContact += DeltaTime;
	}

	// Grace window: contact is gone, but for a short while the enemy still
	// knows where the survivor really is. The remembered location keeps
	// following the live position, so when the window closes the trail points
	// around the corner the survivor took, not at the door they left through.
	const bool bInTrackingGrace =
		!bHasContact && TimeSinceLastContact <= Definition->TrackingGraceSeconds;

	const bool bIsTracking = bHasContact || bInTrackingGrace;

	if (bInTrackingGrace && EnemyPerceptionComponent)
	{
		EnemyPerceptionComponent->SetLastKnownLocation(Target, Target->GetActorLocation());
	}

	if (!bIsTracking)
	{
		// Arriving at the last known location with nothing sensed means the
		// trail is cold: search now instead of standing on the spot for the
		// rest of the memory window. The guard waits for the grace window plus
		// one repath, so the move toward the final remembered location has
		// been issued before an Idle path is read as "arrived".
		const bool bReachedLastKnown =
			TimeSinceLastContact > Definition->TrackingGraceSeconds + ChaseRepathInterval &&
			HasReachedDestination();

		if (bReachedLastKnown || TimeSinceLastContact >= Definition->MemorySeconds)
		{
			UE_LOG(LogNevergone, Log,
				TEXT("[ExplorationEnemyController] %s lost the survivor and is searching the last known location"),
				*GetNameSafe(ControlledEnemy.Get()));

			EnterState(EExplorationEnemyState::Investigate);

			return;
		}
	}

	const bool bRepathDue = TimeSinceRepath < ChaseRepathInterval &&
		TimeSinceRepath + DeltaTime >= ChaseRepathInterval;

	TimeSinceRepath += DeltaTime;

	// Not while crossing a doorway: the repath runs as soon as the enemy is
	// out of it. An enemy paused at a closed door is still at the link
	// start, on the navmesh, and may change its route.
	if (DoorwayCrossing.IsCrossing(*this))
	{
		if (bRepathDue)
		{
			UE_LOG(LogNevergone, Verbose,
				TEXT("[ExplorationEnemyController] %s chase repath postponed while crossing a doorway"),
				*GetNameSafe(ControlledEnemy.Get()));
		}
	}
	else if (TimeSinceRepath >= ChaseRepathInterval)
	{
		TimeSinceRepath = 0.0f;

		// While tracking, chase the live actor. Once the window closes, run to
		// the last remembered location instead of following through walls.
		if (bIsTracking && LineBlock != ECaptureLineBlock::None)
		{
			// The target's own spot, with no overlap: the usual reach test is
			// a straight distance and, close to the door or the wall, would
			// call the move done on this side of it. The path has to run
			// through the door or around the wall.
			MoveToActor(Target, ExplorationEnemyAI::LineBlockedChaseAcceptanceRadius, /*bStopOnOverlap=*/false);
		}
		else if (bIsTracking)
		{
			MoveToActor(Target, ControlledEnemy->GetCatchDistance() * 0.5f);
		}
		else if (EnemyPerceptionComponent)
		{
			FVector TargetLastKnown;

			if (EnemyPerceptionComponent->GetLastKnownLocationOf(Target, TargetLastKnown))
			{
				MoveToLocation(TargetLastKnown, MoveAcceptanceRadius);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Doors
// ---------------------------------------------------------------------------

void AExplorationEnemyController::HandleDoorBarred(ADoor* Door)
{
	if (!ControlledEnemy.IsValid() || !Door)
	{
		return;
	}

	// Every new move that reaches the same door comes back here (a chase
	// repath): the knocking carries on. Coming back to a door knocked at
	// earlier carries on from there too.
	KnockDoor = Door;

	// Doors that opened or broke since are done with.
	for (auto It = DoorKnocks.CreateIterator(); It; ++It)
	{
		const ADoor* Knocked = It.Key().Get();

		if (!Knocked || Knocked->GetState() == EDoorState::Open || Knocked->GetState() == EDoorState::Broken)
		{
			It.RemoveCurrent();
		}
	}

	DoorKnocks.FindOrAdd(Door);
}

void AExplorationEnemyController::TickDoorKnock(float DeltaTime)
{
	ADoor* Door = KnockDoor.Get();

	if (!Door || !Definition || !ControlledEnemy.IsValid())
	{
		return;
	}

	// Unlatched, opened, or broken by somebody else: nothing left to knock
	// at. Only an unlatched door that stays closed keeps the knocking done.
	if (!Door->IsLatched())
	{
		if (Door->GetState() != EDoorState::Closed)
		{
			DoorKnocks.Remove(Door);
		}

		KnockDoor = nullptr;

		return;
	}

	// Away from the door for now (moving elsewhere, or between two move
	// requests that both end here): the knocking so far is kept.
	if (!IsKnocking())
	{
		return;
	}

	FDoorKnock& Knock = DoorKnocks.FindOrAdd(Door);

	if (!Knock.HasStarted())
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemyController] %s starts knocking at %s (breaks it after %.1fs)"),
			*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Door), Door->GetBreakSeconds());
	}

	const FDoorKnock::FStep Step = Knock.Advance(DeltaTime, Definition->KnockIntervalSeconds, Door->GetBreakSeconds());

	if (Step.bKnock)
	{
		ControlledEnemy->AnnounceDoorSignal(EExplorationEnemySignalMoment::DoorKnock, Door);
	}

	if (Step.bBreak)
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemyController] %s breaks %s after %.1fs of knocking"),
			*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Door), Knock.GetElapsedSeconds());

		DoorKnocks.Remove(Door);

		KnockDoor = nullptr;

		// The door lets every agent waiting at it walk on.
		if (Door->TryBreak(ControlledEnemy.Get()))
		{
			ControlledEnemy->AnnounceDoorSignal(EExplorationEnemySignalMoment::DoorBreak, Door);
		}
	}
}

bool AExplorationEnemyController::IsKnocking() const
{
	const ADoor* Door = KnockDoor.Get();

	return Door && Door->IsLatched() && Door->IsAgentWaiting(GetPathFollowingComponent());
}

ECaptureLineBlock AExplorationEnemyController::FindCaptureLineBlock(const AActor* Target, const AActor*& OutBlocker) const
{
	OutBlocker = nullptr;

	const UWorld* World = GetWorld();

	if (!World || !ControlledEnemy.IsValid() || !Target)
	{
		return ECaptureLineBlock::None;
	}

	// Capsule centre to capsule centre, about 96 cm off the floor: furniture
	// lower than that is reached over, walls and door frames are not.
	const FVector From = ControlledEnemy->GetActorLocation();

	const FVector To = Target->GetActorLocation();

	if (const ADoor* Door = ADoor::FindClosedDoorBetween(World, From, To))
	{
		OutBlocker = Door;

		return CaptureRules::ResolveLineBlock(/*bClosedDoorBetween=*/true, /*bSceneryBetween=*/false);
	}

	// Never the visibility channel: the open leaf blocks it. Noise occlusion
	// is blocked by walls and ignored by the leaf, knockable props,
	// characters and ragdolls. Whatever else still stops the trace without
	// being scenery (a closed door's box, a body with custom collision) is
	// skipped and the trace repeated.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExplorationCaptureLine), false);
	Params.AddIgnoredActor(ControlledEnemy.Get());
	Params.AddIgnoredActor(Target);

	bool bSceneryBetween = false;

	FHitResult Hit;

	for (int32 Skipped = 0; Skipped < ExplorationEnemyAI::MaxCaptureLineSkippedHits; ++Skipped)
	{
		if (!World->LineTraceSingleByChannel(Hit, From, To, NevergoneCollision::NoiseOcclusion, Params))
		{
			break;
		}

		const UPrimitiveComponent* HitComponent = Hit.GetComponent();

		if (!HitComponent)
		{
			break;
		}

		const bool bPartOfDoor = Cast<ADoor>(Hit.GetActor()) != nullptr;

		if (CaptureRules::IsSceneryHit(HitComponent->GetCollisionObjectType(), bPartOfDoor))
		{
			bSceneryBetween = true;

			OutBlocker = Hit.GetActor();

			break;
		}

		Params.AddIgnoredComponent(HitComponent);
	}

	return CaptureRules::ResolveLineBlock(/*bClosedDoorBetween=*/false, bSceneryBetween);
}

void AExplorationEnemyController::ForgetDoorKnock()
{
	KnockDoor = nullptr;

	DoorKnocks.Reset();
}

void AExplorationEnemyController::HandleDoorCrossing(ADoor* Door, const FVector& Exit)
{
	DoorwayCrossing.HandleDoorCrossing(Door, Exit);
}

void AExplorationEnemyController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	DoorwayCrossing.HandleMoveCompleted(*this, RequestID, Result);
}

bool AExplorationEnemyController::DoesStateWaitForDoorway(EExplorationEnemyState State)
{
	switch (State)
	{
	case EExplorationEnemyState::Idle:
	case EExplorationEnemyState::Patrol:
	case EExplorationEnemyState::Suspicious:
	case EExplorationEnemyState::Investigate:
		return true;

	default:
		return false;
	}
}

bool AExplorationEnemyController::DeferStateStartToDoorwayExit()
{
	if (bStateStartAfterDoorway)
	{
		return true;
	}

	if (!ControlledEnemy.IsValid() ||
		!DoesStateWaitForDoorway(ControlledEnemy->GetBehaviorState()) ||
		!DoorwayCrossing.IsCrossing(*this))
	{
		return false;
	}

	bStateStartAfterDoorway = true;

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExplorationEnemyController] %s enters %s halfway through a doorway; it starts once out of it"),
		*GetNameSafe(ControlledEnemy.Get()), *UEnum::GetValueAsString(ControlledEnemy->GetBehaviorState()));

	return true;
}

void AExplorationEnemyController::StartStateAfterDoorway()
{
	if (!ControlledEnemy.IsValid())
	{
		return;
	}

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExplorationEnemyController] %s is out of the doorway and starts %s"),
		*GetNameSafe(ControlledEnemy.Get()), *UEnum::GetValueAsString(ControlledEnemy->GetBehaviorState()));

	if (ControlledEnemy->GetBehaviorState() == EExplorationEnemyState::Patrol)
	{
		StartPatrol();
	}
	else
	{
		StopMovement();
	}
}

// ---------------------------------------------------------------------------
// Capture callbacks
// ---------------------------------------------------------------------------

void AExplorationEnemyController::BeginKnockdown(float Seconds, AActor* ChaseTargetAfter)
{
	KnockdownLeft = Seconds;

	KnockdownChaseTarget = ChaseTargetAfter;

	UE_LOG(LogNevergone, Log,
		TEXT("[ExplorationEnemyController] %s knocked down for %.1fs"),
		*GetNameSafe(ControlledEnemy.Get()), Seconds);

	EnterState(EExplorationEnemyState::KnockedDown);
}

void AExplorationEnemyController::TickKnockdown(float DeltaTime)
{
	KnockdownLeft -= DeltaTime;

	if (KnockdownLeft > 0.0f)
	{
		return;
	}

	AActor* Target = KnockdownChaseTarget.Get();

	KnockdownChaseTarget = nullptr;

	if (IsValid(Target) && AExcursionSurvivorNPC::IsSurvivorBody(Target))
	{
		UE_LOG(LogNevergone, Log,
			TEXT("[ExplorationEnemyController] %s got up and is chasing %s again"),
			*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Target));

		// Entering Chase resets the contact timer, so the tracking grace
		// window opens as if contact had just been lost.
		ChaseTarget = Target;

		EnterState(EExplorationEnemyState::Chase);

		return;
	}

	EnterState(EExplorationEnemyState::Patrol);
}

void AExplorationEnemyController::Kill()
{
	UE_LOG(LogNevergone, Log,
		TEXT("[ExplorationEnemyController] %s was killed"),
		*GetNameSafe(ControlledEnemy.Get()));

	EnterState(EExplorationEnemyState::Dead);
}

void AExplorationEnemyController::FinishFeeding()
{
	ChaseTarget = nullptr;

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);

	FVector PlayerLastKnown;

	if (Player && EnemyPerceptionComponent &&
		EnemyPerceptionComponent->GetLastKnownLocationOf(Player, PlayerLastKnown))
	{
		// Makes that point the current stimulus, which is what Investigate
		// walks to.
		EnemyPerceptionComponent->SetLastKnownLocation(Player, PlayerLastKnown);

		UE_LOG(LogNevergone, Log,
			TEXT("[ExplorationEnemyController] %s finished feeding and goes where it last sensed %s"),
			*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Player));

		EnterState(EExplorationEnemyState::Investigate);

		return;
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExplorationEnemyController] %s finished feeding and never sensed the player; returning to patrol"),
		*GetNameSafe(ControlledEnemy.Get()));

	EnterState(EExplorationEnemyState::Patrol);
}

// ---------------------------------------------------------------------------
// Perception handlers
// ---------------------------------------------------------------------------

void AExplorationEnemyController::HandleSurvivorSensed(AActor* Survivor, const FVector& StimulusLocation,
	EExplorationSenseChannel Channel)
{
	if (!ControlledEnemy.IsValid() || !IsValid(Survivor))
	{
		return;
	}

	const EExplorationEnemyState CurrentState = ControlledEnemy->GetBehaviorState();

	// Busy with a capture, on the ground, or dead: nothing sensed matters.
	if (CurrentState == EExplorationEnemyState::Catch ||
		CurrentState == EExplorationEnemyState::KnockedDown ||
		CurrentState == EExplorationEnemyState::Dead)
	{
		return;
	}

	switch (Channel)
	{
	case EExplorationSenseChannel::Smell:
		// A chase keeps its target: smelling someone else on the way does
		// not turn the enemy around.
		if (CurrentState == EExplorationEnemyState::Chase && ChaseTarget.IsValid())
		{
			break;
		}

		// Already close enough to touch. No confirmation window.
		ChaseTarget = Survivor;
		EnterState(EExplorationEnemyState::Chase);
		break;

	case EExplorationSenseChannel::Hearing:
		if (CurrentState == EExplorationEnemyState::Suspicious)
		{
			HoldNoise(StimulusLocation, Survivor);

			break;
		}

		ReactToNoise();
		break;

	case EExplorationSenseChannel::Sight:
		// Suspicion and escalation are both decided in Tick, which sees a
		// survivor standing still in view too.
		break;

	default:
		break;
	}
}

void AExplorationEnemyController::HandleSurvivorLost(AActor* Survivor)
{
	// Chase handles its own memory countdown; nothing to do here beyond
	// letting the last known location stand.
	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExplorationEnemyController] %s lost direct contact with %s"),
		*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Survivor));
}

void AExplorationEnemyController::HandlePlaceNoiseHeard(AActor* Source, const FVector& NoiseLocation)
{
	if (!ControlledEnemy.IsValid())
	{
		return;
	}

	if (ControlledEnemy->GetBehaviorState() == EExplorationEnemyState::Suspicious)
	{
		HoldNoise(NoiseLocation, Source);

		return;
	}

	if (!CanReactToNoise())
	{
		return;
	}

	// Already where the noise points: another enemy knocking at the door
	// this one knocks at, or a place it is already investigating (a knock
	// repeats every few seconds). Restarting would stop and turn it on
	// every knock; the search time is refreshed instead.
	if (IsKnocking() && FVector::Dist(NoiseLocation, KnockDoor->GetNoiseLocation()) <= ExplorationEnemyAI::SamePlaceRadius)
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemyController] %s ignores the place noise of %s: it is knocking at that door"),
			*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Source));

		return;
	}

	if (ControlledEnemy->GetBehaviorState() == EExplorationEnemyState::Investigate &&
		EnemyPerceptionComponent && EnemyPerceptionComponent->HasLastKnownLocation() &&
		FVector::Dist(NoiseLocation, EnemyPerceptionComponent->GetLastKnownLocation()) <= ExplorationEnemyAI::SamePlaceRadius)
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemyController] %s hears %s again where it is investigating; search time refreshed"),
			*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Source));

		InvestigateElapsed = 0.0f;

		return;
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExplorationEnemyController] %s heard a place noise from %s and is investigating %s"),
		*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Source), *NoiseLocation.ToCompactString());

	// Recorded only now: an enemy that ignores the noise keeps its own trail.
	if (EnemyPerceptionComponent)
	{
		EnemyPerceptionComponent->SetMostRecentStimulus(NoiseLocation);
	}

	ReactToNoise();
}

bool AExplorationEnemyController::CanReactToNoise() const
{
	if (!ControlledEnemy.IsValid())
	{
		return false;
	}

	// A noise never confirms a target, it only moves the enemy to look. A
	// chase keeps its target, a suspicious enemy is already looking at one
	// and holds the noise instead (HoldNoise), and a captured, downed or
	// dead one cannot react.
	const EExplorationEnemyState State = ControlledEnemy->GetBehaviorState();

	return State == EExplorationEnemyState::Idle ||
		State == EExplorationEnemyState::Patrol ||
		State == EExplorationEnemyState::Investigate;
}

void AExplorationEnemyController::HoldNoise(const FVector& NoiseLocation, const AActor* Source)
{
	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExplorationEnemyController] %s is suspicious and holds the noise of %s at %s%s"),
		*GetNameSafe(ControlledEnemy.Get()), *GetNameSafe(Source), *NoiseLocation.ToCompactString(),
		Suspicion.HasHeldNoise() ? TEXT(", replacing an older one") : TEXT(""));

	Suspicion.HoldNoise(NoiseLocation);
}

void AExplorationEnemyController::ReactToNoise()
{
	if (!CanReactToNoise())
	{
		return;
	}

	if (ControlledEnemy->GetBehaviorState() == EExplorationEnemyState::Investigate)
	{
		// A new noise mid-search re-targets the investigation to it.
		BeginInvestigation();
	}
	else
	{
		EnterState(EExplorationEnemyState::Investigate);
	}
}