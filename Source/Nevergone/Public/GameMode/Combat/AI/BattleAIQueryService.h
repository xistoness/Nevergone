// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/BattleTypes.h"
#include "Types/AIBattleTypes.h"
#include "BattleAIQueryService.generated.h"

class ACharacterBase;
class UBattleGameplayAbility;

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
	TArray<ACharacterBase*> GetValidTargetsForAbility(const ACharacterBase* Unit, TSubclassOf<UBattleGameplayAbility> AbilityClass) const;

	float EstimateDamage(const ACharacterBase* SourceUnit, const ACharacterBase* TargetUnit, TSubclassOf<UBattleGameplayAbility> AbilityClass) const;
	float GetDistanceScore(const ACharacterBase* SourceUnit, const ACharacterBase* TargetUnit) const;

	bool IsTileReserved(const FIntPoint& Tile, const FTeamTurnContext& TurnContext) const;
	float GetReservedDamageForTarget(const ACharacterBase* Target, const FTeamTurnContext& TurnContext) const;

private:
	bool DoesUnitBelongToTeam(const ACharacterBase* Unit, EBattleUnitTeam Team) const;

private:
	UPROPERTY()
	TArray<TObjectPtr<AActor>> Combatants;
};
