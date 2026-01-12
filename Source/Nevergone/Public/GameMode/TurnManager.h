// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TurnManager.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnTurnAdvanced);

UCLASS()
class NEVERGONE_API UTurnManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const TArray<AActor*>& InCombatants);
	void StartTurns();
	void AdvanceTurn();

	FOnTurnAdvanced OnTurnAdvanced;

private:
	int32 CurrentTurnIndex = INDEX_NONE;
	TArray<TWeakObjectPtr<AActor>> Combatants;
	
};
