// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "NevergoneCharacter.h"
#include "Interfaces/SaveParticipant.h"
#include "CharacterBase.generated.h"

class UInputMappingContext;
class UInputAction;
class USaveableComponent;
class UInteractableComponent;
class UCharacterModeComponent;
class UExplorationModeComponent;
class UBattleModeComponent;
class UUnitStatsComponent;
class UMyAbilitySystemComponent;

UCLASS()
class NEVERGONE_API ACharacterBase : public ANevergoneCharacter, public ISaveParticipant
{
	GENERATED_BODY()

public:

	ACharacterBase();
	
	void EnableExplorationMode();
	void EnableBattleMode();
	
	// Camera
	void ResetCamera();

	// Save system
	virtual void WriteSaveData_Implementation(FActorSaveData& OutData) const override;
	virtual void ReadSaveData_Implementation(const FActorSaveData& InData) override;
	virtual void OnPostRestore_Implementation() override;
	
	// Getters
	UUnitStatsComponent* GetUnitStats() const;
	
	UBattleModeComponent* GetBattleModeComponent() const;
	
	UMyAbilitySystemComponent* GetAbilitySystemComponent();
	
	const FVector& GetPendingMoveLocation() const;
	
	// Setters
	
	void SetPendingMoveLocation(const FVector& InLocation);
	
	void MoveToLocation(const FVector& InLocation);
	
	

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	void SetMode(UCharacterModeComponent* NewMode);
		
	UPROPERTY()
	FVector PendingMoveLocation;
	
	// Camera persistence
	float ArmLength;
	FRotator ControllerRot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Modes")
	UExplorationModeComponent* ExplorationMode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Modes")
	UBattleModeComponent* BattleMode;
	
	UPROPERTY()
	UCharacterModeComponent* ActiveMode;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	USaveableComponent* SaveableComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	UUnitStatsComponent* UnitStatsComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UMyAbilitySystemComponent* AbilitySystemComponent;
};
