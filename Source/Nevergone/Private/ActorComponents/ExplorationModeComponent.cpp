// Copyright Xyzto Works


#include "ActorComponents/ExplorationModeComponent.h"

#include "ActorComponents/InteractableComponent.h"
#include "Characters/CharacterBase.h"
#include "Debug/DebugDrawHelper.h"
#include "EngineTools/CollisionChannels.h"
#include "GameInstance/MyGameInstance.h"

UExplorationModeComponent::UExplorationModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UExplorationModeComponent::EnterMode()
{
	Super::EnterMode();
	UE_LOG(LogTemp, Warning, TEXT("Trying to enter mode..."));
	SetComponentTickEnabled(true);	
}

void UExplorationModeComponent::ExitMode()
{
	Super::ExitMode();
	UE_LOG(LogTemp, Warning, TEXT("Trying to exit mode..."));
	SetComponentTickEnabled(false);

	if (HighlightedInteractable)
	{
		HighlightedInteractable->SetHighlighted(false);
		HighlightedInteractable = nullptr;
	}
}

void UExplorationModeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateInteractionTrace();
}

void UExplorationModeComponent::Input_Interact()
{
	UE_LOG(LogTemp, Warning, TEXT("Trying to interact..."));
	if (!HighlightedInteractable)
	{
		return;
	}

	HighlightedInteractable->Interact(GetOwner());
}

void UExplorationModeComponent::Input_Save()
{
	if (UMyGameInstance* GI = GetOwner()->GetGameInstance<UMyGameInstance>())
	{
		GI->RequestSaveGame();
	}
}

void UExplorationModeComponent::Input_Load()
{
	if (UMyGameInstance* GI = GetOwner()->GetGameInstance<UMyGameInstance>())
	{
		GI->RequestLoadGame();
	}
}

void UExplorationModeComponent::UpdateInteractionTrace()
{
	// Get camera direction only
	ACharacter* Owner = Cast<ACharacter>(GetOwner());
	
	if (!Owner) { return; }
	if (!Owner->GetController()) { return; }
	
	FVector CameraLocation;
	FRotator CameraRotation;
	Owner->GetController()->GetPlayerViewPoint(CameraLocation, CameraRotation);
	const FVector TraceDirection = CameraRotation.Vector();

	const FVector TraceStart = Owner->GetMesh()->GetSocketLocation(TEXT("HeadSocket"));
	const FVector TraceEnd = TraceStart + (TraceDirection * MaxInteractionDistance);
	
	// Debug visualization
	//DebugDrawHelper::DrawLine(TraceStart, TraceEnd, FColor::Green, 1.5f);
	//DebugDrawHelper::DrawSphere(TraceStart, 6.0f, FColor::Blue, 1.5f);

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
