// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/BattleInputContext.h"
#include "Types/BattleTypes.h"
#include "UObject/Object.h"
#include "CombatManager.generated.h"

class UBattlePreparationContext;
class UBattleInputContextBuilder;
class UTurnManager;
class UBattleInputManager;
class ABattleCameraPawn;
class ACharacterBase;
class UBattleState;
class UGridManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatFinished, EBattleUnitTeam, WinningTeam);

UCLASS()
class NEVERGONE_API UCombatManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize();
	void EnterPreparation(UBattlePreparationContext& BattlePrepContext);
	
	/** Spawn units and send to TurnManager */
	void StartCombat(UBattlePreparationContext& BattlePrepContext);
	void InitializeSpawnedUnitForBattle(ACharacterBase* Character, EBattleUnitTeam Team, UGridManager* Grid);
	void HandleTurnStateChanged(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase);
	void HandleUnitDeath(ACharacterBase* DeadUnit);
	bool IsTeamDefeated(EBattleUnitTeam Team) const;
	void UpdateInputContext();

	void CancelPreparation();
	void EndCombatWithWinner(EBattleUnitTeam WinningTeam);
	
	/** Input / camera integration */
	void RegisterBattleCamera(ABattleCameraPawn* InCameraPawn);
	void SelectNextControllableUnit();
	void SelectPreviousControllableUnit();
	UBattleInputManager* GetBattleInputManager() const { return BattleInputManager; }
	ABattleCameraPawn* GetBattleCameraPawn() const { return BattleCameraPawn; }

	FOnCombatFinished OnCombatFinished;

protected:
	
	void RestoreTeamActionPoints(EBattleUnitTeam Team);
	
	UPROPERTY(BlueprintReadOnly)
	TArray<ACharacterBase*> SpawnedAllies;
	UPROPERTY(BlueprintReadOnly)
	TArray<ACharacterBase*> SpawnedEnemies;

private:
	void Cleanup();
	
	/** Turn notifications */
	void OnPlayerTurnStarted();
	void OnEnemyTurnStarted();
	void EndPlayerTurn();
	void EndEnemyTurn();

	void SelectFirstAvailableUnit();
	void HandleActiveUnitChanged(ACharacterBase* NewActiveUnit);
	
	UFUNCTION()
	void HandleUnitOutOfAP(ACharacterBase* Unit);
	
	
	// Debug
	static void LogActivePlayerController(UWorld* World, const FString& Context);
	void BindToSelectedUnitBattleEvents(ACharacterBase* Unit);
	void UnbindFromSelectedUnitBattleEvents(ACharacterBase* Unit);
	
	UFUNCTION()
	void HandleUnitActionStarted();
	UFUNCTION()
	void HandleUnitActionFinished();

private:	
	
	UPROPERTY()
	int32 SelectedUnit;
	
	UPROPERTY()
	ACharacterBase* CurrentSelectedUnit;
	
	UPROPERTY()
	UTurnManager* TurnManager = nullptr;
	
	UPROPERTY()
	UBattleInputManager* BattleInputManager = nullptr;

	UPROPERTY()
	ABattleCameraPawn* BattleCameraPawn = nullptr;	
	
	UPROPERTY()
	UBattleInputContextBuilder* InputContextBuilder = nullptr;
	
	UPROPERTY()
	UBattleState* BattleState = nullptr;
	
	UPROPERTY()
	bool bCombatEnding = false;
};
