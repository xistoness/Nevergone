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

void UInteractableComponent::SetHighlighted(bool bHighlighted)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	TArray<UPrimitiveComponent*> Primitives;
	OwnerActor->GetComponents<UPrimitiveComponent>(Primitives);

	for (UPrimitiveComponent* Primitive : Primitives)
	{
		
		UE_LOG(LogTemp, Warning,
		TEXT("[Highlight] Primitive: %s | Class: %s | Visible: %d"),
		*Primitive->GetName(),
		*Primitive->GetClass()->GetName(),
		Primitive->IsVisible()
		);

		UE_LOG(LogTemp, Warning, TEXT("Tries to highlight..."));
		Primitive->SetRenderCustomDepth(bHighlighted);
		Primitive->SetCustomDepthStencilValue(1);
		
		UE_LOG(LogTemp, Warning,
			TEXT("[Highlight] CustomDepth ENABLED for: %s"),
			*Primitive->GetName()
		);
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

