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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatFinished, EBattleUnitTeam, WinningTeam);

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

	FOnCombatFinished OnCombatFinished;

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
	// Debug
	static void LogActivePlayerController(UWorld* World, const FString& Context);
	
	UFUNCTION()
	void HandleUnitActionStarted(ACharacterBase* ActingUnit);
	UFUNCTION()
	void HandleUnitActionFinished(ACharacterBase* ActingUnit);

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

	// Central event bus for damage, heal, status and death notifications.
	// Created at StartCombat and injected into every unit's BattleModeComponent.
	UPROPERTY()
	UCombatEventBus* EventBus = nullptr;

	UPROPERTY()
	bool bCombatEnding = false;
};