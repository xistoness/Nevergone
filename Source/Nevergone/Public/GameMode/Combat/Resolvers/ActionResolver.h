// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/BattleTypes.h"
#include "ActionResolver.generated.h"


class UBattleGameplayAbility;

UCLASS()
class NEVERGONE_API UActionResolver : public UObject
{
	GENERATED_BODY()
	
public:
	FActionResult ResolvePreview(TSubclassOf<UBattleGameplayAbility> AbilityClass, const FActionContext& Context);
	FActionResult ResolveExecution(TSubclassOf<UBattleGameplayAbility> AbilityClass, const FActionContext& Context);
};
