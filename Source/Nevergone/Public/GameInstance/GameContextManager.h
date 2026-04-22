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
	void ReturnToExploration(bool bForce = false);

    /**
     * Called when a save slot is loaded. If the save was written mid-combat,
     * reconstructs the battle session directly — bypassing BattlePreparation.
     */
    void HandleSaveLoaded();

	
	UCombatManager* GetActiveCombatManager() const { return ActiveCombatManager; }

	ACharacterBase* GetSavedExplorationCharacter() const;
	TSubclassOf<ACharacterBase> GetExplorationCharacterClass() const;
	FTransform GetSavedExplorationTransform() const;
	float GetSavedExplorationArmLength() const;
	
	UBattlePreparationContext* GetActivePrepContext() const;
	UBattleResultsContext* GetActiveResultsContext() const;
	EGameContextState GetCurrentState() const { return CurrentState; }
	FRotator GetSavedExplorationControlRotation() const;
	
	ACharacterBase* SpawnExplorationActorFromClass(TSubclassOf<ACharacterBase> CharacterClass);

	
	void ClearBattleSession();
	void DestroyExplorationCharacter(ACharacterBase* Character);
	TSubclassOf<ACharacterBase> ResolveExplorationPawnClass(TSubclassOf<ACharacterBase> FallbackClass) const;
	
	// Called by PartyManagerSubsystem::OnLeaderChanged during exploration —
	// destroys the current pawn and spawns the new leader's pawn at same position.
	void HandleLeaderChanged();
	void InitializeSubsystem();

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
	
	void SyncExplorationActorHP(ACharacterBase* ExplorationActor);
	
	// Spawns a fresh exploration actor from the saved session data.
	// Used by ReturnToExploration after combat ends.
	ACharacterBase* SpawnExplorationActor();

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