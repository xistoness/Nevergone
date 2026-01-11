// Copyright Xyzto Works


#include "ActorComponents/InteractableComponent.h"
#include "Interfaces/Interactable.h"
#include "EngineTools/CollisionChannels.h"

UInteractableComponent::UInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInteractableComponent::Interact(AActor* Interactor)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;
	
	if (!IsValid(Interactor))
		return;
	
	if (Owner->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
	{
		IInteractable::Execute_Interact(Owner, Interactor);
	}
}

void UInteractableComponent::OnRegister()
{
	Super::OnRegister();
	AActor* Owner = GetOwner();
	if (!Owner)
		return;

	TArray<UPrimitiveComponent*> Primitives;
	Owner->GetComponents<UPrimitiveComponent>(Primitives);

	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (!Primitive)
			continue;

		// Only affect collision-enabled components
		if (Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			continue;

		Primitive->SetCollisionResponseToChannel(
			NevergoneCollision::Interactable,
			ECollisionResponse::ECR_Block
		);
	}
}

