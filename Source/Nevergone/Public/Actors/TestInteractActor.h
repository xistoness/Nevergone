// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Interactable.h"
#include "Interfaces/SaveParticipant.h"
#include "TestInteractActor.generated.h"

class UInteractableComponent;
class USaveableComponent;

UCLASS()
class NEVERGONE_API ATestInteractActor : public AActor, public IInteractable, public ISaveParticipant
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATestInteractActor();
	virtual void Interact_Implementation(AActor* Interactor) override;


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interaction")
	UInteractableComponent* InteractableComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	USaveableComponent* SaveableComponent;
	
	virtual void WriteSaveData_Implementation(FActorSaveData& OutData) const override;
	virtual void ReadSaveData_Implementation(const FActorSaveData& InData) override;
	
	UPROPERTY(EditAnywhere)
	int32 InteractCount = 0;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
