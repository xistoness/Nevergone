// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/BattleInputContext.h"
#include "TurnIndicatorWidget.generated.h"

class UTurnManager;
class UTextBlock;

/**
 * Simple banner widget that shows "PLAYER TURN" or "ENEMY TURN".
 *
 * Subscribes to UTurnManager::OnTurnStateChanged and updates its label
 * and color accordingly. All visual styling is done in the Blueprint subclass.
 */
UCLASS(Abstract)
class NEVERGONE_API UTurnIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/**
	 * Binds this widget to the turn manager.
	 * Call immediately after AddToViewport.
	 */
	UFUNCTION(BlueprintCallable, Category = "Turn Indicator")
	void InitializeWithTurnManager(UTurnManager* InTurnManager);

	/** Removes the turn state delegate. Call before RemoveFromParent. */
	UFUNCTION(BlueprintCallable, Category = "Turn Indicator")
	void Deinitialize();

protected:

	/** Label that shows "PLAYER TURN" / "ENEMY TURN". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TurnLabel;

	/**
	 * Called whenever the turn owner changes.
	 * Implement color changes, animations, or sound cues in Blueprint.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Turn Indicator")
	void OnTurnOwnerChanged(bool bIsPlayerTurn);

private:

	void HandleTurnStateChanged(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase);

	UPROPERTY()
	TObjectPtr<UTurnManager> TurnManager;

	FDelegateHandle TurnHandle;
};