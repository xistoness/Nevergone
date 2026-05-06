// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "NevergonePlayerController.h"
#include "BattlePreparationController.generated.h"

class UBattlePreparationContext;
class UBattlePreparationWidget;
class UInputAction;
class UInventoryWidget;

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

	// ReceivedPlayer is called by the engine after LocalPlayer is assigned to
	// this controller. Using it instead of BeginPlay guarantees that SetInputMode
	// and CreateWidget never fire while LocalPlayer is still null (which happens
	// when SpawnActor triggers BeginPlay before SwapPlayerControllers runs).
	virtual void ReceivedPlayer() override;
	virtual void OnPossess(APawn* InPawn) override;

	/** Widget class — assign the BP subclass in the controller's Blueprint CDO. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UBattlePreparationWidget> PreparationWidgetClass;

	// Input actions — assign in Blueprint CDO.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ConfirmInput;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CancelInput;

	// Toggles the inventory overlay during battle preparation.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InventoryInput;

	// Cycles to the previous/next party member in the inventory's right panel.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InventoryPrevCharInput;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InventoryNextCharInput;

	// Shared UI navigation inputs — reuse the same assets as ExplorationPlayerController.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> UIUpInput;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> UIDownInput;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> UILeftInput;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> UIRightInput;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> UIConfirmInput;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> UICancelInput;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UInventoryWidget> InventoryWidgetClass;

	UPROPERTY()
	TObjectPtr<UInventoryWidget> InventoryWidget;

private:

	/** Creates and populates the preparation widget from the stored context. */
	void ShowPreparationWidget();

	void SendRequestBattleStart();
	void SendRequestAbortBattle();
	void RequestAbortBattle();

	void OnConfirm(const FInputActionValue& Value);
	void OnCancel(const FInputActionValue& Value);
	void OnInventory(const FInputActionValue& Value);
	void OnInventoryPrevChar(const FInputActionValue& Value);
	void OnInventoryNextChar(const FInputActionValue& Value);
	void OnInventoryNavigateUp(const FInputActionValue& Value);
	void OnInventoryNavigateDown(const FInputActionValue& Value);
	void OnInventoryNavigateLeft(const FInputActionValue& Value);
	void OnInventoryNavigateRight(const FInputActionValue& Value);
	void OnInventoryConfirm(const FInputActionValue& Value);
	void OnInventoryCancel(const FInputActionValue& Value);

	void CreateInventoryWidget();

	UFUNCTION()
	void HandleStartBattleClicked();
	void HandleAbortBattleClicked();

	UPROPERTY()
	TObjectPtr<UBattlePreparationContext> PreparationContext;

	UPROPERTY()
	TObjectPtr<UBattlePreparationWidget> PreparationWidget;
};