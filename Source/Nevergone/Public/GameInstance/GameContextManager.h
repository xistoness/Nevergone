// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/BattleTypes.h"
#include "GameContextManager.generated.h"

class ATowerFloorGameMode;
class UCombatManager;
class UBattlePreparationContext;
class UGridManager;
class AFloorEncounterVolume;
class UBattleResultsContext;

enum class EGameContextState : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameContextChanged, EGameContextState);

UCLASS()
class NEVERGONE_API UGameContextManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** State control */
	
	void RequestInitialState(EGameContextState InitialState);
	void RequestExploration();
	void RequestBattlePreparation(class AFloorEncounterVolume* EncounterSource);
	void RequestBattleStart();
	void RequestBattleEnd();
	void RequestTransition();
	void EnterBattleResults();
	void ReturnToExploration();
	void AbortBattle();
	
	ACharacterBase* GetSavedExplorationCharacter() const;
	FTransform GetSavedExplorationTransform() const;
	
	UBattlePreparationContext* GetActivePrepContext() const;
	UBattleResultsContext* GetActiveResultsContext() const;
	EGameContextState GetCurrentState() const { return CurrentState; }
	FRotator GetSavedExplorationControlRotation() const;
	
	void ClearBattleSession();
	void DestroyExplorationCharacter(ACharacterBase* Character);
	
	FOnGameContextChanged OnGameContextChanged;

private:
	bool CanEnterState(EGameContextState TargetState) const;
	void EnterState(EGameContextState NewState);
	void ExitState(EGameContextState OldState);
	
	void CreateCombatManager();
	
	UFUNCTION()
	void HandleCombatFinished(EBattleUnitTeam WinningTeam);

	ATowerFloorGameMode* GetActiveFloorGameMode() const;

private:
	EGameContextState CurrentState = EGameContextState::None;
	
	UPROPERTY()
	UCombatManager* ActiveCombatManager = nullptr;
	UPROPERTY()
	UBattlePreparationContext* ActiveBattlePrepContext = nullptr;
	
	// Active battle session
	UPROPERTY()
	FBattleSessionData ActiveBattleSession;
	
	UPROPERTY()
	UBattleResultsContext* ActiveResultsContext = nullptr;
};
