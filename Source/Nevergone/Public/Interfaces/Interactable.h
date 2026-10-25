// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class NEVERGONE_API IInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void Interact(AActor* Interactor);

	// Holding interact: only for C++ implementers. When CanInteractHold is
	// true for the interactor, a press is held back: held long enough it
	// becomes InteractHold, released earlier it is a plain Interact.
	virtual bool CanInteractHold(const AActor* Interactor) const { return false; }

	virtual void InteractHold(AActor* Interactor) {}
};
