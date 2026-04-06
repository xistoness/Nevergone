// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleUnitState.h"
#include "BattleState.generated.h"

class ACharacterBase;
class UBattlePreparationContext;

UCLASS()
class NEVERGONE_API UBattleState : public UObject
{
	GENERATED_BODY()

public:
	/* ----- Lifecycle ----- */
	void Initialize(UBattlePreparationContext& BattlePrepContext);
	bool CanUnitAct(ACharacterBase* Unit);
	bool IsUnitExhausted(ACharacterBase* Unit);

	FBattleUnitState* FindUnitState(ACharacterBase* Unit);
	const FBattleUnitState* FindUnitState(ACharacterBase* Unit) const;

	/* ----- Mutations ----- */
	void ResetTurnStateForTeam(EBattleUnitTeam Team);

	void MarkUnitAsActed(ACharacterBase* Unit);
	void MarkUnitMoved(ACharacterBase* Unit);
	void ConsumeActionPoints(ACharacterBase* Unit, int32 Amount);

	void ApplyDamage(ACharacterBase* Unit, int32 Amount);
	void ApplyHeal(ACharacterBase* Unit, int32 Amount);

	/**
	 * Read-only access to all unit states.
	 * Used by BattleHUDWidget to iterate combatants when spawning HP bars.
	 */
	const TArray<FBattleUnitState>& GetAllUnitStates() const { return UnitStates; }

	/* ----- Result ----- */

	/**
	 * Writes battle outcome back to each unit's persistent state.
	 * Must be called before actors are destroyed (i.e. before Cleanup).
	 *
	 * Currently persists:
	 *   - HP: written to UnitStatsComponent::PersistentHP so it carries
	 *     into the next exploration session. Dead units are written as 0.
	 *
	 * Future additions (XP, persistent debuffs) should be added here.
	 */
	void PersistToCombatants();

	// Kept for compatibility — calls PersistToCombatants internally.
	void GenerateResult();

private:
	UPROPERTY()
	TArray<FBattleUnitState> UnitStates;

	FBattleUnitState* ActiveTurnUnit = nullptr;
};