// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "NevergoneCharacter.h"
#include "Interfaces/SaveParticipant.h"
#include "CharacterBase.generated.h"

class UInputMappingContext;
class UInputAction;
class USaveableComponent;

UCLASS()
class NEVERGONE_API ACharacterBase : public ANevergoneCharacter, public ISaveParticipant
{
	GENERATED_BODY()

public:

	ACharacterBase();

	virtual void WriteSaveData_Implementation(FActorSaveData& OutData) const override;
	virtual void ReadSaveData_Implementation(const FActorSaveData& InData) override;
	virtual void OnPostRestore_Implementation() override;
	void ResetCamera();

protected:

	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnInteractionPressed();
	UFUNCTION()
	void OnSaveGamePressed();
	UFUNCTION()
	void OnLoadGamePressed();
	
	// Persistent runtime GUID (mirrors SaveData)
	UPROPERTY(VisibleAnywhere, Category="Save")
	FGuid SaveGuid;
	
	float ArmLength;
	FRotator ControllerRot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	USaveableComponent* SaveableComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* InteractionInput;
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* SaveGameInput;
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* LoadGameInput;
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
