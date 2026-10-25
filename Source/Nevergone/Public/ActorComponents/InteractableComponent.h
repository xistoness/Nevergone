// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractableComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class NEVERGONE_API UInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UInteractableComponent();
	
	void Interact(AActor* Interactor);

	// Forwarded to the owner's IInteractable hold hooks (C++ owners only).
	bool CanInteractHold(const AActor* Interactor) const;

	void InteractHold(AActor* Interactor);
	
	void SetHighlighted(bool bHighlighted);

protected:
	virtual void OnRegister() override;		
};
