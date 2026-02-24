// Copyright Xyzto Works

#pragma once

#include "../../../../../../../../../UE_5.7/Engine/Source/Runtime/Core/Public/CoreMinimal.h"
#include "../../../../../../../../../UE_5.7/Engine/Source/Runtime/Engine/Public/Subsystems/GameInstanceSubsystem.h"
#include "GameContextManager.generated.h"

class ATowerFloorGameMode;
class UCombatManager;
class UBattlePreparationContext;
class UGridManager;
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

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameContextChanged, EGameContextState);

UCLASS()
class NEVERGONE_API UGameContextManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** State control */
	void RequestExploration();
	void RequestBattlePreparation(class AFloorEncounterVolume* EncounterSource);
	void RequestBattleStart();
	void RequestBattleEnd();
	void RequestTransition();
	void AbortBattle();
	
	EGameContextState GetCurrentState() const { return CurrentState; }
	FOnGameContextChanged OnGameContextChanged;

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
