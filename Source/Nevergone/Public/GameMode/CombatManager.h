// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CombatManager.generated.h"

class UBattlePreparationContext;
class UTurnManager;

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
	
	void CancelPreparation();
	void EndCombat();

	FOnCombatFinished OnCombatFinished;

protected:
	UPROPERTY(BlueprintReadOnly)
	TArray<TObjectPtr<AActor>> SpawnedAllies;
	UPROPERTY(BlueprintReadOnly)
	TArray<TObjectPtr<AActor>> SpawnedEnemies;

private:
	void Cleanup();

private:	
	UPROPERTY()
	UTurnManager* TurnManager = nullptr;
	
	
};
