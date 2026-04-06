// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "NevergonePlayerController.h"
#include "BattlePreparationController.generated.h"

class UBattlePreparationContext;
class UBattlePreparationWidget;
class UInputAction;

/**
 * Player controller active during the BattlePreparation state.
 *
 * Responsibilities:
 *   - Shows BattlePreparationWidget populated from the preparation context
 *   - Blocks all gameplay input (cursor-only, UI mode)
 *   - Listens to the widget's OnStartBattleClicked and calls
 *     GameContextManager::RequestBattleStart()
 *   - Supports keyboard Confirm / Cancel shortcuts (Confirm = start, Cancel = abort)
 *
 * Setup:
 *   - Assign PreparationWidgetClass in the Blueprint CDO.
 *   - Assign ConfirmInput and CancelInput input actions in the Blueprint CDO.
 */
UCLASS()
class NEVERGONE_API ABattlePreparationController : public ANevergonePlayerController
{
	GENERATED_BODY()

public:

	virtual void SetupInputComponent() override;

	/**
	 * Called by TowerFloorGameMode immediately after this controller is spawned.
	 * Stores the context and shows the preparation widget.
	 */
	void SetPreparationContext(UBattlePreparationContext* InContext);

protected:

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

	/** Widget class — assign the BP subclass in the controller's Blueprint CDO. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UBattlePreparationWidget> PreparationWidgetClass;

	// Input actions — assign in Blueprint CDO.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ConfirmInput;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CancelInput;

private:

	/** Creates and populates the preparation widget from the stored context. */
	void ShowPreparationWidget();

	void SendRequestBattleStart();
	void SendRequestAbortBattle();
	void RequestAbortBattle();

	void OnConfirm(const FInputActionValue& Value);
	void OnCancel(const FInputActionValue& Value);

	UFUNCTION()
	void HandleStartBattleClicked();
	void HandleAbortBattleClicked();

	UPROPERTY()
	TObjectPtr<UBattlePreparationContext> PreparationContext;

	UPROPERTY()
	TObjectPtr<UBattlePreparationWidget> PreparationWidget;
};