// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/BattleInputContext.h"
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

DECLARE_MULTICAST_DELEGATE(FOnCombatFinished);

UCLASS()
class NEVERGONE_API UCombatManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize();
	void EnterPreparation(UBattlePreparationContext& BattlePrepContext);
	
	/** Spawn units and send to TurnManager */
	void StartCombat(UBattlePreparationContext& BattlePrepContext);
	void HandleTurnStateChanged(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase);
	void UpdateInputContext();

	void CancelPreparation();
	void EndCombat();
	
	/** Input / camera integration */
	void RegisterBattleCamera(ABattleCameraPawn* InCameraPawn);
	void SelectNextControllableUnit();
	void SelectPreviousControllableUnit();
	UBattleInputManager* GetBattleInputManager() const { return BattleInputManager; }
	ABattleCameraPawn* GetBattleCameraPawn() const { return BattleCameraPawn; }

	FOnCombatFinished OnCombatFinished;

protected:
	UPROPERTY(BlueprintReadOnly)
	TArray<TObjectPtr<AActor>> SpawnedAllies;
	UPROPERTY(BlueprintReadOnly)
	TArray<TObjectPtr<AActor>> SpawnedEnemies;

private:
	void Cleanup();
	
	/** Turn notifications */
	void OnPlayerTurnStarted();
	void OnEnemyTurnStarted();
	void SelectFirstAvailableUnit();
	void HandleActiveUnitChanged(ACharacterBase* NewActiveUnit);
	void HandleUnitOutOfAP(ACharacterBase* Unit);
	void EndPlayerTurn();
	
	// Debug
	static void LogActivePlayerController(UWorld* World, const FString& Context);

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
};
