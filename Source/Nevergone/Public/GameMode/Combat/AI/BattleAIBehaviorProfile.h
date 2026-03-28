// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BattleAIBehaviorProfile.generated.h"

class UBattleAIScoringRule;

UCLASS(BlueprintType)
class NEVERGONE_API UBattleAIBehaviorProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Instanced)
	TArray<TObjectPtr<UBattleAIScoringRule>> ScoringRules;
};
