// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "ExcursionExit.generated.h"

class UInteractableComponent;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// AExcursionExit
//
// The building's way out. Interacting with it ends the excursion; the result
// screen follows. Placed by hand, one per building.
//
// Survivors count as rescued only if they are following at that moment: one
// left waiting inside stays inside.
//
// Component layout mirrors ADoor:
//   Root (USceneComponent)
//   ├── ExitMesh          (UStaticMeshComponent — visual and blocker)
//   └── InteractionSphere (USphereComponent     — QueryOnly trace volume)
// ---------------------------------------------------------------------------
UCLASS(Blueprintable)
class NEVERGONE_API AExcursionExit : public AActor, public IInteractable
{
	GENERATED_BODY()

public:

	AExcursionExit();

	// --- IInteractable ---
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ExitMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInteractableComponent> InteractableComponent;
};
