// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/BattleTypes.h"
#include "BattleResultsWidget.generated.h"

class UTextBlock;
class UButton;

/**
 * End-of-battle screen shown after every combat encounter.
 *
 * Displays:
 *   - Victory / Defeat header
 *   - Surviving allies count
 *   - Surviving enemies count (0 on victory)
 *   - Allies lost during the fight
 *
 * The "Continue" button calls OnContinueClicked, which the owning
 * BattlePlayerController binds to trigger ReturnToExploration.
 *
 * All visual styling and animations are handled in the Blueprint subclass.
 */
UCLASS(Abstract)
class NEVERGONE_API UBattleResultsWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/**
	 * Populates and shows the results screen.
	 *
	 * @param WinningTeam       Which team won (Ally = victory, Enemy = defeat)
	 * @param SurvivingAllies   Number of player units still alive
	 * @param SurvivingEnemies  Number of enemy units still alive (0 on victory)
	 * @param AlliesLost        Total player units that died this fight
	 */
	UFUNCTION(BlueprintCallable, Category = "Battle Results")
	void ShowResults(EBattleUnitTeam WinningTeam,
	                 int32 SurvivingAllies,
	                 int32 SurvivingEnemies,
	                 int32 AlliesLost);

	/**
	 * Delegate fired when the player presses "Continue".
	 * Bind this in BattlePlayerController to trigger ReturnToExploration.
	 */
	DECLARE_MULTICAST_DELEGATE(FOnContinueClicked);
	FOnContinueClicked OnContinueClicked;

protected:

	// -----------------------------------------------------------------------
	// Named UMG widgets — bind in the Blueprint designer
	// -----------------------------------------------------------------------

	/** "VICTORY" or "DEFEAT" header text. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ResultHeaderText;

	/** e.g. "Surviving Allies: 3" */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SurvivingAlliesText;

	/** e.g. "Allies Lost: 1" */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AlliesLostText;

	/** Button to dismiss this screen and return to exploration. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ContinueButton;

	// -----------------------------------------------------------------------
	// Blueprint events
	// -----------------------------------------------------------------------

	/** Called after ShowResults populates the text fields. Animate here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle Results")
	void OnResultsReady(bool bIsVictory);

	// -----------------------------------------------------------------------
	// UMG click handler
	// -----------------------------------------------------------------------

	UFUNCTION()
	void HandleContinueClicked();

	virtual void NativeConstruct() override;
};