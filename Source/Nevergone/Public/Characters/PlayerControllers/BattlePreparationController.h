// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "NevergonePlayerController.h"
#include "BattlePreparationController.generated.h"

class UInputAction;

UCLASS()
class NEVERGONE_API ABattlePreparationController : public ANevergonePlayerController
{
	GENERATED_BODY()
public:
	virtual void SetupInputComponent() override;
	void OnConfirm(const FInputActionValue& Value);
	void OnCancel(const FInputActionValue& Value);
	
	// Input Actions
	UPROPERTY(EditDefaultsOnly, Category="Input")
	UInputAction* ConfirmInput;
	
	UPROPERTY(EditDefaultsOnly, Category="Input")
	UInputAction* CancelInput;
	
	
protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	
	//Debug functions
	void SendRequestBattleStart();
	void RequestAbortBattle();
};
