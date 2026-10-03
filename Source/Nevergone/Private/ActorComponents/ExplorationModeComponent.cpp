// Copyright Xyzto Works


#include "ActorComponents/ExplorationModeComponent.h"
#include "ActorComponents/ExcursionThrowComponent.h"

#include "Nevergone.h"
#include "ActorComponents/InteractableComponent.h"
#include "Characters/CharacterBase.h"
#include "EngineTools/CollisionChannels.h"
#include "Localization/NevergoneGameUserSettings.h"
#include "World/ExcursionNoiseSubsystem.h"

#include "GameFramework/CharacterMovementComponent.h"

UExplorationModeComponent::UExplorationModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UExplorationModeComponent::EnterMode()
{
	Super::EnterMode();

	SetComponentTickEnabled(true);

	bSprintRequested = false;
	bSprinting = false;
	bInputSuspended = false;
	RecoveryDelayLeft = 0.0f;
	PantingTimeLeft = 0.0f;

	// A fresh excursion starts rested. Stamina is never saved.
	Stamina = MaxStamina;

	if (UCharacterMovementComponent* Movement = GetMovement())
	{
		Movement->NavAgentProps.bCanCrouch = true;
		Movement->MaxWalkSpeedCrouched = CrouchSpeed;
		Movement->SetCrouchedHalfHeight(CrouchedHalfHeight);
	}

	ApplyMovementSpeed();

	UE_LOG(LogNevergone, Log,
		TEXT("[ExplorationModeComponent] %s entered exploration mode"),
		*GetNameSafe(GetOwner()));
}

void UExplorationModeComponent::ExitMode()
{
	Super::ExitMode();

	SetComponentTickEnabled(false);

	bSprintRequested = false;
	SetSprinting(false);

	bInputSuspended = false;

	if (UExcursionThrowComponent* Throw = GetThrowComponent())
	{
		Throw->CancelAim();
	}

	PantingTimeLeft = 0.0f;

	if (ACharacter* Character = GetCharacter())
	{
		if (Character->bIsCrouched)
		{
			Character->UnCrouch();
		}
	}

	if (HighlightedInteractable)
	{
		HighlightedInteractable->SetHighlighted(false);
		HighlightedInteractable = nullptr;
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExplorationModeComponent] %s left exploration mode"),
		*GetNameSafe(GetOwner()));
}

void UExplorationModeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInputSuspended)
	{
		UpdateInteractionTrace();
	}

	UpdateStamina(DeltaTime);
	UpdatePanting(DeltaTime);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void UExplorationModeComponent::Input_Move(float Right, float Forward)
{
	if (ANevergoneCharacter* Character = Cast<ANevergoneCharacter>(GetOwner()))
	{
		Character->DoMove(Right, Forward);
	}
}

void UExplorationModeComponent::Input_JumpStart()
{
	if (ANevergoneCharacter* Character = Cast<ANevergoneCharacter>(GetOwner()))
	{
		Character->DoJumpStart();
	}
}

void UExplorationModeComponent::Input_JumpStop()
{
	if (ANevergoneCharacter* Character = Cast<ANevergoneCharacter>(GetOwner()))
	{
		Character->DoJumpEnd();
	}
}

void UExplorationModeComponent::Input_SprintPressed()
{
	bSprintRequested = true;
}

void UExplorationModeComponent::Input_SprintReleased()
{
	bSprintRequested = false;
}

void UExplorationModeComponent::Input_CrouchPressed()
{
	ACharacter* Character = GetCharacter();

	if (!Character)
	{
		return;
	}

	const UNevergoneGameUserSettings* Settings = UNevergoneGameUserSettings::GetNevergoneGameUserSettings();

	// Toggle is the default, and the only behaviour when settings are missing.
	const bool bToggle = !Settings || Settings->IsCrouchToggle();

	if (bToggle && Character->bIsCrouched)
	{
		Character->UnCrouch();
	}
	else
	{
		Character->Crouch();
	}
}

void UExplorationModeComponent::Input_CrouchReleased()
{
	const UNevergoneGameUserSettings* Settings = UNevergoneGameUserSettings::GetNevergoneGameUserSettings();

	if (Settings && !Settings->IsCrouchToggle())
	{
		if (ACharacter* Character = GetCharacter())
		{
			Character->UnCrouch();
		}
	}
}

void UExplorationModeComponent::Input_Interact()
{
	if (!HighlightedInteractable)
	{
		return;
	}

	HighlightedInteractable->Interact(GetOwner());
}

void UExplorationModeComponent::Input_ThrowPressed()
{
	if (UExcursionThrowComponent* Throw = GetThrowComponent())
	{
		Throw->BeginAim();
	}
}

void UExplorationModeComponent::Input_ThrowReleased()
{
	if (UExcursionThrowComponent* Throw = GetThrowComponent())
	{
		Throw->ReleaseAim();
	}
}

UExcursionThrowComponent* UExplorationModeComponent::GetThrowComponent() const
{
	const AActor* Owner = GetOwner();

	return Owner ? Owner->FindComponentByClass<UExcursionThrowComponent>() : nullptr;
}

void UExplorationModeComponent::SetInputSuspended(bool bSuspended)
{
	if (bInputSuspended == bSuspended)
	{
		return;
	}

	bInputSuspended = bSuspended;

	if (bInputSuspended)
	{
		// A sprint held when the UI opened must not resume by itself when it
		// closes.
		bSprintRequested = false;
		SetSprinting(false);

		// Same for a held aim: its release would never arrive, and a screen
		// or a capture is no moment to throw.
		if (UExcursionThrowComponent* Throw = GetThrowComponent())
		{
			Throw->CancelAim();
		}

		if (HighlightedInteractable)
		{
			HighlightedInteractable->SetHighlighted(false);
			HighlightedInteractable = nullptr;
		}
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExplorationModeComponent] %s input %s"),
		*GetNameSafe(GetOwner()), bInputSuspended ? TEXT("suspended") : TEXT("resumed"));
}

// ---------------------------------------------------------------------------
// Stamina
// ---------------------------------------------------------------------------

float UExplorationModeComponent::GetStaminaFraction() const
{
	return MaxStamina > 0.0f ? Stamina / MaxStamina : 0.0f;
}

void UExplorationModeComponent::UpdateStamina(float DeltaTime)
{
	ACharacter* Character = GetCharacter();

	if (!Character)
	{
		return;
	}

	// Sprinting out of a crouch stands the survivor up first.
	if (bSprintRequested && Character->bIsCrouched && Stamina >= MinStaminaToSprint)
	{
		Character->UnCrouch();
	}

	// Holding the button while standing still neither sprints nor drains.
	const bool bMoving = Character->GetVelocity().Size2D() > 10.0f;

	const bool bWantsSprint = bSprintRequested && bMoving && !Character->bIsCrouched;

	if (!bSprinting && bWantsSprint && Stamina >= MinStaminaToSprint)
	{
		SetSprinting(true);
	}
	else if (bSprinting && !bWantsSprint)
	{
		SetSprinting(false);
	}

	if (bSprinting)
	{
		Stamina -= StaminaDrainPerSecond * DeltaTime;

		RecoveryDelayLeft = StaminaRecoveryDelaySeconds;

		if (Stamina <= 0.0f)
		{
			Stamina = 0.0f;

			SetSprinting(false);

			StartPanting();
		}

		return;
	}

	if (RecoveryDelayLeft > 0.0f)
	{
		RecoveryDelayLeft -= DeltaTime;

		return;
	}

	Stamina = FMath::Min(MaxStamina, Stamina + StaminaRecoveryPerSecond * DeltaTime);
}

void UExplorationModeComponent::StartPanting()
{
	PantingTimeLeft = PantingDurationSeconds;

	// Emit on the next tick rather than after a full interval.
	NextPantingIn = 0.0f;

	UE_LOG(LogNevergone, Log,
		TEXT("[ExplorationModeComponent] %s ran out of stamina and started panting"),
		*GetNameSafe(GetOwner()));
}

void UExplorationModeComponent::UpdatePanting(float DeltaTime)
{
	if (PantingTimeLeft <= 0.0f)
	{
		return;
	}

	PantingTimeLeft -= DeltaTime;

	NextPantingIn -= DeltaTime;

	if (NextPantingIn > 0.0f)
	{
		return;
	}

	NextPantingIn = PantingIntervalSeconds;

	AActor* Owner = GetOwner();

	if (UExcursionNoiseSubsystem* Noise = UExcursionNoiseSubsystem::Get(this))
	{
		// Breathing has no surface: it does not come off the floor.
		Noise->EmitNoise(PantingNoise, Owner->GetActorLocation(), Owner);
	}
}

void UExplorationModeComponent::SetSprinting(bool bNewSprinting)
{
	if (bSprinting == bNewSprinting)
	{
		return;
	}

	bSprinting = bNewSprinting;

	ApplyMovementSpeed();

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExplorationModeComponent] %s sprinting: %d (stamina %.0f)"),
		*GetNameSafe(GetOwner()), bSprinting ? 1 : 0, Stamina);
}

void UExplorationModeComponent::ApplyMovementSpeed()
{
	if (UCharacterMovementComponent* Movement = GetMovement())
	{
		Movement->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
	}
}

ACharacter* UExplorationModeComponent::GetCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

UCharacterMovementComponent* UExplorationModeComponent::GetMovement() const
{
	const ACharacter* Character = GetCharacter();

	return Character ? Character->GetCharacterMovement() : nullptr;
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------

void UExplorationModeComponent::UpdateInteractionTrace()
{
	// Get camera direction only
	ACharacter* Owner = Cast<ACharacter>(GetOwner());
	
	if (!Owner) { return; }

	// Only the player's survivor looks for things to interact with. A
	// follower runs this same component and would otherwise light up
	// everything it walks past.
	if (!Owner->IsPlayerControlled())
	{
		if (HighlightedInteractable)
		{
			HighlightedInteractable->SetHighlighted(false);
			HighlightedInteractable = nullptr;
		}

		return;
	}

	if (!Owner->GetController()) { return; }
	
	FVector CameraLocation;
	FRotator CameraRotation;
	Owner->GetController()->GetPlayerViewPoint(CameraLocation, CameraRotation);
	const FVector TraceDirection = CameraRotation.Vector();

	const FVector TraceStart = Owner->GetMesh()->GetSocketLocation(TEXT("HeadSocket"));
	const FVector TraceEnd = TraceStart + (TraceDirection * MaxInteractionDistance);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		TraceStart,
		TraceEnd,
		NevergoneCollision::Interactable,
		Params
	);

	UInteractableComponent* NewInteractable = nullptr;

	if (bHit && Hit.GetActor())
	{
		NewInteractable =
			Hit.GetActor()->FindComponentByClass<UInteractableComponent>();
	}

	// If highlight target changed
	if (NewInteractable != HighlightedInteractable)
	{
		// Clear previous highlight
		if (HighlightedInteractable)
		{
			HighlightedInteractable->SetHighlighted(false);
		}

		HighlightedInteractable = NewInteractable;

		// Apply new highlight
		if (HighlightedInteractable)
		{
			HighlightedInteractable->SetHighlighted(true);
		}
	}
}
