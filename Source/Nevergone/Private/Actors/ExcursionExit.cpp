// Copyright Xyzto Works

#include "Actors/ExcursionExit.h"

#include "Nevergone.h"
#include "ActorComponents/InteractableComponent.h"
#include "World/ExcursionSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

AExcursionExit::AExcursionExit()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ExitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExitMesh"));
	ExitMesh->SetupAttachment(Root);

	// Trace-only volume, so the prompt works from a comfortable distance
	// whatever the mesh's own collision is. InteractableComponent promotes
	// the Interactable channel to Block on registration.
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(Root);
	InteractionSphere->SetSphereRadius(110.0f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);

	InteractableComponent = CreateDefaultSubobject<UInteractableComponent>(TEXT("InteractableComponent"));
}

void AExcursionExit::Interact_Implementation(AActor* Interactor)
{
	UExcursionSubsystem* Excursion = UExcursionSubsystem::Get(this);

	if (!Excursion)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionExit] %s: no UExcursionSubsystem in this world"),
			*GetNameSafe(this));

		return;
	}

	if (Excursion->HasEnded())
	{
		return;
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionExit] %s used by %s"),
		*GetNameSafe(this), *GetNameSafe(Interactor));

	Excursion->ExitExcursion();
}
