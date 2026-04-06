// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "NevergonePlayerController.h"
#include "BattleResultsController.generated.h"

class UBattleResultsContext;
class UBattleResultsWidget;

/**
 * Player controller active during the BattleResults state.
 *
 * Responsibilities:
 *   - Shows the BattleResultsWidget with cursor enabled
 *   - Blocks all gameplay input (no pawn possessed, no movement)
 *   - Listens to the widget's OnContinueClicked and calls
 *     GameContextManager::ReturnToExploration when the player confirms
 *
 * Scalability notes:
 *   - SetResultsContext() is called by TowerFloorGameMode before the widget
 *     is shown, so subclasses or the BP can read all results data from it.
 *   - Add party navigation input here when building the per-character results view.
 */
UCLASS()
class NEVERGONE_API ABattleResultsController : public ANevergonePlayerController
{
	GENERATED_BODY()

public:

	/**
	 * Called by TowerFloorGameMode immediately after this controller is spawned.
	 * Stores the context and shows the results widget.
	 */
	void SetResultsContext(UBattleResultsContext* InContext);

protected:

	virtual void BeginPlay() override;

	// Widget class — assign the BP subclass in the controller's Blueprint CDO.
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UBattleResultsWidget> ResultsWidgetClass;

private:

	void ShowResultsWidget();

	UFUNCTION()
	void HandleContinueClicked();

	UPROPERTY()
	TObjectPtr<UBattleResultsContext> ResultsContext;

	UPROPERTY()
	TObjectPtr<UBattleResultsWidget> ResultsWidget;
};