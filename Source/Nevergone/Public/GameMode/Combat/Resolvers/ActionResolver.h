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

private:
	// Returns the live instanced ability if the source actor has an ASC with a
	// granted spec for AbilityClass. Falls back to the CDO if not found, so AI
	// previews (which have no instanced ability) continue to work correctly.
	const UBattleGameplayAbility* ResolveAbilityObject(
		TSubclassOf<UBattleGameplayAbility> AbilityClass,
		const FActionContext& Context
	) const;
};