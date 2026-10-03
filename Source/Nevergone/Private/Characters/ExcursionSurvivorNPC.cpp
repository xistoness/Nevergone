// Copyright Xyzto Works

#include "Characters/ExcursionSurvivorNPC.h"

#include "Nevergone.h"
#include "Characters/PlayerControllers/ExcursionSurvivorController.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

AExcursionSurvivorNPC::AExcursionSurvivorNPC()
{
	// Placed survivors must be possessed in the level, not only when spawned.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	AIControllerClass = AExcursionSurvivorController::StaticClass();
}

void AExcursionSurvivorNPC::BeginPlay()
{
	Super::BeginPlay();

	// Entering the mode is what applies the speeds and the crouched height to
	// the movement component and starts stamina full, exactly as it does for
	// the player's survivor.
	EnableExplorationMode();

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionSurvivorNPC] %s ready, waiting to be recruited"),
		*GetNameSafe(this));
}

void AExcursionSurvivorNPC::Interact_Implementation(AActor* Interactor)
{
	if (SurvivorState == EExcursionSurvivorState::Dead)
	{
		return;
	}

	if (!IsRecruited())
	{
		// Not recruited yet: talking is the only way in.
		Super::Interact_Implementation(Interactor);

		return;
	}

	ToggleFollow();
}

void AExcursionSurvivorNPC::Recruit()
{
	if (IsRecruited())
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExcursionSurvivorNPC] %s is already recruited; Recruit ignored"),
			*GetNameSafe(this));

		return;
	}

	SetSurvivorState(EExcursionSurvivorState::Following);
}

void AExcursionSurvivorNPC::ToggleFollow()
{
	switch (SurvivorState)
	{
	case EExcursionSurvivorState::Following:
		SetSurvivorState(EExcursionSurvivorState::Waiting);
		break;

	case EExcursionSurvivorState::Waiting:
		SetSurvivorState(EExcursionSurvivorState::Following);
		break;

	default:
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionSurvivorNPC] %s cannot toggle follow before being recruited"),
			*GetNameSafe(this));
		break;
	}
}

void AExcursionSurvivorNPC::MarkDead()
{
	if (SurvivorState == EExcursionSurvivorState::Dead)
	{
		return;
	}

	// Leaving the mode stops the stamina tick, so a corpse never pants.
	SetMode(nullptr);

	SetSurvivorState(EExcursionSurvivorState::Dead);
}

bool AExcursionSurvivorNPC::IsSurvivorBody(const AActor* Actor)
{
	if (const AExcursionSurvivorNPC* Survivor = Cast<AExcursionSurvivorNPC>(Actor))
	{
		return Survivor->IsRecruited();
	}

	const APawn* Pawn = Cast<APawn>(Actor);

	return Pawn && Pawn->IsPlayerControlled();
}

void AExcursionSurvivorNPC::GatherSurvivorBodies(const UWorld* World, TArray<AActor*>& OutBodies)
{
	OutBodies.Reset();

	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();

		if (APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr)
		{
			OutBodies.Add(PlayerPawn);
		}
	}

	for (TActorIterator<AExcursionSurvivorNPC> It(World); It; ++It)
	{
		if (It->IsRecruited())
		{
			OutBodies.Add(*It);
		}
	}
}

void AExcursionSurvivorNPC::SetSurvivorState(EExcursionSurvivorState NewState)
{
	if (SurvivorState == NewState)
	{
		return;
	}

	const EExcursionSurvivorState OldState = SurvivorState;

	SurvivorState = NewState;

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionSurvivorNPC] %s state %s -> %s"),
		*GetNameSafe(this),
		*UEnum::GetValueAsString(OldState),
		*UEnum::GetValueAsString(NewState));

	OnSurvivorStateChanged.Broadcast(this, NewState);
}
