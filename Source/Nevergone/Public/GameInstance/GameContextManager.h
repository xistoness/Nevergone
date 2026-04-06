// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameInstance/MySaveGame.h"
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
	void RequestAbortBattle();
	void EnterBattleResults();
	void ReturnToExploration();

    /**
     * Called when a save slot is loaded. If the save was written mid-combat,
     * reconstructs the battle session directly — bypassing BattlePreparation.
     */
    void HandleSaveLoaded();

	
	UCombatManager* GetActiveCombatManager() const { return ActiveCombatManager; }

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

    /**
     * Rebuilds a combat session from FSavedCombatSession without going through
     * BattlePreparation. Finds the EncounterVolume by guid, rebuilds the party
     * from the saved encounter data, spawns units, and calls StartCombat with
     * InitializeFromSave so volatile state (HP, AP, status effects, cooldowns)
     * is restored correctly.
     */
    void RequestBattleCombatRestore(const FSavedCombatSession& SavedSession);

	/**
	 * Spawns the saved exploration character and broadcasts
	 * OnGameContextChanged(Exploration) unconditionally — bypassing the
	 * CurrentState == NewState early-return in EnterState.
	 *
	 * Use this when aborting battle prep while CurrentState is already
	 * Exploration (e.g. the no-spawn-zone guard). In that case EnterState
	 * would silently no-op, leaving the ExplorationPlayerController without
	 * a possessed pawn.
	 */
	void ForceReenterExploration();
	
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

    // Set by RequestBattleCombatRestore after combat is fully wired.
    // Read by RequestInitialState so TowerFloorGameMode::BeginPlay enters
    // Battle instead of whatever InitialContextState the GameMode requested.
    bool bPendingCombatRestore = false;
};