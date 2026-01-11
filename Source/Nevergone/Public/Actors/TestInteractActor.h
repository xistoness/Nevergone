// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Interactable.h"
#include "GameInstance/SaveableActorBase.h"
#include "TestInteractActor.generated.h"

UCLASS()
class NEVERGONE_API ATestInteractActor : public ASaveableActorBase, public IInteractable
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATestInteractActor();
	virtual void Interact_Implementation(AActor* Interactor) override;


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual void WriteSaveData_Implementation(FActorSaveData& OutData) const override;
	virtual void ReadSaveData(const FActorSaveData& InData) override;
	
	UPROPERTY(EditAnywhere)
	int32 InteractCount = 0;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
