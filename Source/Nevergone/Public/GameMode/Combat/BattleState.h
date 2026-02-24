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
	bool CanUnitAct(ACharacterBase* Unit) const;
	bool IsUnitExhausted(ACharacterBase* Unit) const;
	
	
	FBattleUnitState* FindUnitState(ACharacterBase* Unit);
	const FBattleUnitState* FindUnitState(ACharacterBase* Unit) const;
	
	/* ----- Mutations ----- */
	void ResetTurnStateForTeam(EBattleUnitTeam Team);
	
	void MarkUnitAsActed(ACharacterBase* Unit);
	void MarkUnitMoved(ACharacterBase* Unit);
	void ConsumeActionPoints(ACharacterBase* Unit, int32 Amount);
	
	void ApplyDamage(ACharacterBase* Unit, float Amount);
	void ApplyStatusTag(ACharacterBase* Unit, const FGameplayTag& StatusTag);
	void ClearStatusTag(ACharacterBase* Unit, const FGameplayTag& StatusTag);

	/* ----- Result ----- */
	
	void GenerateResult();
	
private:
	/* ----- Internal storage ----- */

	UPROPERTY()
	TArray<FBattleUnitState> UnitStates;

	FBattleUnitState* ActiveTurnUnit = nullptr;
	
};
