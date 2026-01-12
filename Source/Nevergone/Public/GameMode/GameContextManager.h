// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameContextManager.generated.h"

class ATowerFloorGameMode;
class UCombatManager;
class UBattlePreparationContext;
class AFloorEncounterVolume;

UENUM(BlueprintType)
enum class EGameContextState : uint8
{
	None,
	Exploration,
	BattlePreparation,
	Battle,
	Transition
};

UCLASS()
class NEVERGONE_API UGameContextManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	/** State control */
	void RequestExploration();
	void RequestBattlePreparation(class AFloorEncounterVolume* EncounterSource);
	void RequestBattleStart();
	void RequestBattleEnd();
	void RequestTransition();
	void AbortBattle();
	
	EGameContextState GetCurrentState() const { return CurrentState; }

private:
	bool CanEnterState(EGameContextState TargetState) const;
	void EnterState(EGameContextState NewState);
	void ExitState(EGameContextState OldState);
	
	void CreateCombatManager();
	void DestroyCombatManager();
	
	void HandleCombatFinished();

	ATowerFloorGameMode* GetActiveFloorGameMode() const;

private:
	EGameContextState CurrentState = EGameContextState::None;
	
	UPROPERTY()
	UCombatManager* ActiveCombatManager = nullptr;
	UPROPERTY()
	UBattlePreparationContext* ActiveBattlePrepContext = nullptr;
	
};
