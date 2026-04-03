// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/BattleInputContext.h"
#include "Types/BattleTypes.h"
#include "UObject/Object.h"
#include "CombatManager.generated.h"

class UBattlePreparationContext;
class UBattleInputContextBuilder;
class UCombatEventBus;
class UTurnManager;
class UBattleInputManager;
class ABattleCameraPawn;
class ACharacterBase;
class UBattleState;
class UGridManager;
class UBattleTeamAIPlanner;
class UActionHotbar;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatFinished, EBattleUnitTeam, WinningTeam);

/**
 * Fired when the player-controlled active unit changes.
 * Passes the newly selected unit, or nullptr when no unit is active
 * (e.g. between turns or after combat ends).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveUnitChanged, ACharacterBase*, NewActiveUnit);

/**
 * Fired when the enemy turn begins.
 * The hotbar (and any other player-facing UI) should hide at this point.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnemyTurnBegan);

UCLASS()
class NEVERGONE_API UCombatManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize();
	void EnterPreparation(UBattlePreparationContext& BattlePrepContext);
	void SpawnAllies(UBattlePreparationContext& BattlePrepContext, UGridManager* Grid);
	void SpawnEnemies(UBattlePreparationContext& BattlePrepContext, UGridManager* Grid);

	/** Spawn units and send to TurnManager */
	void StartCombat(UBattlePreparationContext& BattlePrepContext);
	void InitializeSpawnedUnitForBattle(ACharacterBase* Character, EBattleUnitTeam Team, UGridManager* Grid);
	void HandleTurnStateChanged(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase);
	void HandleUnitDeath(ACharacterBase* DeadUnit);
	bool IsTeamDefeated(EBattleUnitTeam Team) const;
	void UpdateInputContext();

	void CancelPreparation();
	void ClearCurrentSelectedUnit();
	void EndCombatWithWinner(EBattleUnitTeam WinningTeam);
	
	UFUNCTION(BlueprintCallable, Category="Combat")
	int32 GetAliveAllies() const;

	UFUNCTION(BlueprintCallable, Category="Combat")
	int32 GetAliveEnemies() const;

	/** Returns total number of ally units spawned at battle start (alive + dead). */
	UFUNCTION(BlueprintCallable, Category="Combat")
	int32 GetTotalAllies() const { return SpawnedAllies.Num(); }

	// -----------------------------------------------------------------------
	// Session object accessors — used by BattleHUDWidget and other observers
	// -----------------------------------------------------------------------

	UBattleState*    GetBattleState()      const { return BattleState; }
	UTurnManager*    GetTurnManager()      const { return TurnManager; }
	UCombatEventBus* GetCombatEventBus()   const { return EventBus; }
	
	/** Input / camera integration */
	void RegisterBattleCamera(ABattleCameraPawn* InCameraPawn);
	void SelectNextControllableUnit();
	void SelectPreviousControllableUnit();
	UBattleInputManager* GetBattleInputManager() const { return BattleInputManager; }
	ABattleCameraPawn* GetBattleCameraPawn() const { return BattleCameraPawn; }

	void Cleanup();
	void BindToCombatUnitActionEvents(ACharacterBase* Unit);
	void UnbindFromCombatUnitActionEvents(ACharacterBase* Unit);
	void RequestEndEnemyTurn();

	// -----------------------------------------------------------------------
	// Public delegates
	// -----------------------------------------------------------------------

	/** Broadcast when combat ends (ally or enemy victory). */
	FOnCombatFinished OnCombatFinished;

	/**
	 * Broadcast whenever the active player unit changes.
	 * Passes nullptr when no unit is selected (e.g. during enemy turn).
	 */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnActiveUnitChanged OnActiveUnitChanged;

	/**
	 * Broadcast at the start of every enemy turn, before any AI actions run.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnEnemyTurnBegan OnEnemyTurnBegan;

protected:
	
	void RestoreTeamActionPoints(EBattleUnitTeam Team);
	
	UPROPERTY(BlueprintReadOnly)
	TArray<ACharacterBase*> SpawnedAllies;
	UPROPERTY(BlueprintReadOnly)
	TArray<ACharacterBase*> SpawnedEnemies;

private:
	
	/** Turn notifications */
	void OnPlayerTurnStarted();
	void OnEnemyTurnStarted();
	void EndPlayerTurn();
	void EndEnemyTurn();

	void SelectFirstAvailableUnit();
	void HandleActiveUnitChanged(ACharacterBase* NewActiveUnit);
	
	UFUNCTION()
	void HandleUnitOutOfAP(ACharacterBase* Unit);

	void FocusCameraOnActingEnemy(ACharacterBase* ActingUnit);
	static void LogActivePlayerController(UWorld* World, const FString& Context);
	
	UFUNCTION()
	void HandleUnitActionStarted(ACharacterBase* ActingUnit);
	UFUNCTION()
	void HandleUnitActionFinished(ACharacterBase* ActingUnit);
	void RestoreInputAfterAction(ACharacterBase* ActingUnit);

private:	
	
	UPROPERTY()
	int32 SelectedUnit;
	
	UPROPERTY()
	ACharacterBase* CurrentSelectedUnit;
	
	UPROPERTY()
	UTurnManager* TurnManager = nullptr;
	
	UPROPERTY()
	UBattleTeamAIPlanner* BattleTeamAIPlanner = nullptr;
	
	UPROPERTY()
	UBattleInputManager* BattleInputManager = nullptr;

	UPROPERTY()
	ABattleCameraPawn* BattleCameraPawn = nullptr;	
	
	UPROPERTY()
	UBattleInputContextBuilder* InputContextBuilder = nullptr;
	
	UPROPERTY()
	UBattleState* BattleState = nullptr;

	UPROPERTY()
	UCombatEventBus* EventBus = nullptr;

	UPROPERTY()
	bool bCombatEnding = false;
	
	FTimerHandle PostActionDelayHandle;
	FTimerHandle AICameraWaitHandle;

	bool bWaitingForPathFinish = false;

	void HandleAIActionFinished();
	void PollCameraReachedActor();

};