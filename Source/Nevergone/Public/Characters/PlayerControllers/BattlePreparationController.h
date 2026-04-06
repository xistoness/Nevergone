// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "NevergonePlayerController.h"
#include "BattlePreparationController.generated.h"

class UBattlePreparationContext;
class UInputAction;

UCLASS()
class NEVERGONE_API ABattlePreparationController : public ANevergonePlayerController
{
	GENERATED_BODY()
public:
	virtual void SetupInputComponent() override;
	void SetPreparationContext(UBattlePreparationContext* InContext);
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
	
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> PreparationWidgetClass;
	
	//Debug functions
	void SendRequestBattleStart();
	void RequestAbortBattle();
	
private:
	UPROPERTY()
	TObjectPtr<UBattlePreparationContext> PreparationContext;

	UPROPERTY()
	TObjectPtr<UUserWidget> PreparationWidget;
};
