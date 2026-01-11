// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "NevergoneCharacter.h"
#include "GameInstance/Saveable.h"
#include "CharacterBase.generated.h"

class UInputMappingContext;
class UInputAction;

UCLASS()
class NEVERGONE_API ACharacterBase : public ANevergoneCharacter, public ISaveable
{
	GENERATED_BODY()

public:

	ACharacterBase();
	
	// ISaveable
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual FGuid GetOrCreateGuid() override;
	virtual void SetActorGuid(const FGuid& NewGuid) override;

	virtual void WriteSaveData_Implementation(FActorSaveData& OutData) const override;
	virtual void ReadSaveData(const FActorSaveData& InData) override;
	virtual TSoftClassPtr<AActor> GetActorClass() const override;

protected:

	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnActionPressed();
	
	// Persistent runtime GUID (mirrors SaveData)
	UPROPERTY(VisibleAnywhere, Category="Save")
	FGuid SaveGuid;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* InteractionInput;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
