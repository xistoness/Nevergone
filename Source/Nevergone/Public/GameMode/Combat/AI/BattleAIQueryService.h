// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/BattleTypes.h"
#include "Types/AIBattleTypes.h"
#include "BattleAIQueryService.generated.h"

class ACharacterBase;
class UAbilityDefinition;
class UBattleGameplayAbility;
class UBattleState;
struct FBattleUnitState;

UCLASS()
class NEVERGONE_API UBattleAIQueryService : public UObject
{
	GENERATED_BODY()
	
public:
	void Initialize(const TArray<AActor*>& InCombatants);

	TArray<ACharacterBase*> GetAlliedUnitsForTeam(EBattleUnitTeam Team) const;
	TArray<ACharacterBase*> GetEnemyUnitsForTeam(EBattleUnitTeam Team) const;

	bool CanUnitStillAct(const ACharacterBase* Unit) const;
	bool IsUnitAlive(const ACharacterBase* Unit) const;
	bool AreUnitsAllies(const ACharacterBase* UnitA, const ACharacterBase* UnitB) const;

	TArray<FIntPoint> GetReachableTiles(const ACharacterBase* Unit) const;

	// AbilityDefinition is passed explicitly because the CDO does not have it set —
	// the project uses a template-class pattern where the same C++ ability class (e.g. GA_Attack_Melee)
	// is shared across many AbilityDefinitions. All per-ability data lives in the Definition asset.
	TArray<ACharacterBase*> GetValidTargetsForAbility(
		const ACharacterBase* Unit,
		TSubclassOf<UBattleGameplayAbility> AbilityClass,
		const UAbilityDefinition* Definition) const;

	float EstimateDamage(
		const ACharacterBase* SourceUnit,
		const ACharacterBase* TargetUnit,
		TSubclassOf<UBattleGameplayAbility> AbilityClass,
		const UAbilityDefinition* Definition) const;
	float GetDistanceScore(const ACharacterBase* SourceUnit, const ACharacterBase* TargetUnit) const;

	bool IsTileReserved(const FIntPoint& Tile, const FTeamTurnContext& TurnContext) const;
	float GetReservedDamageForTarget(const ACharacterBase* Target, const FTeamTurnContext& TurnContext) const;
	
	UBattleState* GetBattleState() const { return BattleState; }
	void SetBattleState(UBattleState* InBattleState);

private:
	bool DoesUnitBelongToTeam(const ACharacterBase* Unit, EBattleUnitTeam Team) const;

private:
	UPROPERTY()
	TArray<TObjectPtr<AActor>> Combatants;
	
	UPROPERTY()
	TObjectPtr<UBattleState> BattleState;
};