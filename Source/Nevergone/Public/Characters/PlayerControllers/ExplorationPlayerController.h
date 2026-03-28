// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "NevergonePlayerController.h"
#include "ExplorationPlayerController.generated.h"

class UInputAction;

UCLASS()
class NEVERGONE_API AExplorationPlayerController : public ANevergonePlayerController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;
	void ApplyExplorationInputMode();
	void OnLook(const FInputActionValue& Value);

	// Input Actions
	UPROPERTY(EditDefaultsOnly, Category="Input")
	UInputAction* LookInput;
	
	UPROPERTY(EditDefaultsOnly, Category="Input")
	UInputAction* InteractionInput;

	UPROPERTY(EditDefaultsOnly, Category="Input|Debug")
	UInputAction* SaveGameInput;

	UPROPERTY(EditDefaultsOnly, Category="Input|Debug")
	UInputAction* LoadGameInput;

	// Handlers
	void HandleInteract();
	
	/** Debug input */
	void HandleSave();
	void HandleLoad();
	
protected:
	
	UPROPERTY(EditAnywhere, Category = "Input|Look")
	float MouseSensitivity = 0.75f;
	
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
};
