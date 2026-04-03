// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/BattleTypes.h"
#include "Types/BattleInputContext.h"
#include "BattleHUDWidget.generated.h"

class UCombatManager;
class UCombatEventBus;
class UTurnManager;
class ACharacterBase;
class UBattleState;
class UUnitHPBarWidget;
class UTurnIndicatorWidget;
class UBattleResultsWidget;
class UVerticalBox;

/**
 * Master in-battle HUD.
 *
 * Responsibilities:
 *   - Spawns and positions a UUnitHPBarWidget above every combatant
 *   - Hosts a UTurnIndicatorWidget ("PLAYER TURN / ENEMY TURN")
 *   - Listens to UCombatManager::OnCombatFinished and shows UBattleResultsWidget
 *
 * Lifecycle:
 *   Created by BattlePlayerController after StartCombat.
 *   Destroyed by BattlePlayerController on Cleanup.
 */
UCLASS(Abstract)
class NEVERGONE_API UBattleHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/**
	 * Wires this HUD to the active combat session.
	 * Must be called immediately after AddToViewport.
	 *
	 * @param InCombatManager  Owning combat manager for this battle
	 */
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void InitializeWithCombatManager(UCombatManager* InCombatManager);

	/** Tears down all delegate bindings. Called before RemoveFromParent. */
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void Deinitialize();

protected:

	// -----------------------------------------------------------------------
	// Widget class references — set in the Blueprint subclass
	// -----------------------------------------------------------------------

	/** Widget class used for each unit's HP bar floating above them. */
	UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Classes")
	TSubclassOf<UUnitHPBarWidget> HPBarWidgetClass;

	/** Widget class used for the turn indicator banner. */
	UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Classes")
	TSubclassOf<UTurnIndicatorWidget> TurnIndicatorClass;

	/** Widget class shown when combat ends (victory or defeat). */
	UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Classes")
	TSubclassOf<UBattleResultsWidget> BattleResultsClass;

	// -----------------------------------------------------------------------
	// Blueprint-bound named widgets (bind in UMG designer)
	// -----------------------------------------------------------------------

	/** Container in the UMG canvas that receives the turn indicator. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> TurnIndicatorContainer;

	// -----------------------------------------------------------------------
	// Blueprint events — implement visual reactions in Blueprint subclass
	// -----------------------------------------------------------------------

	/** Called when the HUD finishes initialization and HP bars are ready. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle HUD")
	void OnHUDInitialized();

	/** Called every time the active turn owner changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle HUD")
	void OnTurnChanged(bool bIsPlayerTurn);

private:

	// -----------------------------------------------------------------------
	// Internal setup helpers
	// -----------------------------------------------------------------------

	/** Creates one UUnitHPBarWidget per combatant and stores them. */
	void SpawnHPBars(UBattleState* BattleState);

	/** Creates and adds the turn indicator to TurnIndicatorContainer. */
	void SpawnTurnIndicator(UTurnManager* TurnManager);

	// -----------------------------------------------------------------------
	// Delegate handlers
	// -----------------------------------------------------------------------

	UFUNCTION()
	void HandleCombatFinished(EBattleUnitTeam WinningTeam);

	void HandleTurnStateChanged(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase);

	// -----------------------------------------------------------------------
	// Private state
	// -----------------------------------------------------------------------

	UPROPERTY()
	TObjectPtr<UCombatManager> CombatManager;

	UPROPERTY()
	TObjectPtr<UTurnIndicatorWidget> TurnIndicatorInstance;

	/** One HP bar widget per unit, keyed by character pointer. */
	UPROPERTY()
	TMap<TObjectPtr<ACharacterBase>, TObjectPtr<UUnitHPBarWidget>> HPBarMap;

	FDelegateHandle TurnStateHandle;
};