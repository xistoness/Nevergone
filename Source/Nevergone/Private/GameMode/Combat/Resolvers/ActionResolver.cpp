// Copyright Xyzto Works


#include "GameMode/Combat/Resolvers/ActionResolver.h"

#include "Characters/Abilities/BattleGameplayAbility.h"

FActionResult UActionResolver::ResolvePreview(TSubclassOf<UBattleGameplayAbility> AbilityClass, const FActionContext& Context)
{
	if (!AbilityClass)
		return {};

	const UBattleGameplayAbility* CDO =
		AbilityClass->GetDefaultObject<UBattleGameplayAbility>();

	
	return CDO ? CDO->BuildPreview(Context) : FActionResult();
}

FActionResult UActionResolver::ResolveExecution(TSubclassOf<UBattleGameplayAbility> AbilityClass,
	const FActionContext& Context)
{
	if (!AbilityClass)
		return {};

	const UBattleGameplayAbility* CDO =
		AbilityClass->GetDefaultObject<UBattleGameplayAbility>();

	return CDO ? CDO->BuildExecution(Context) : FActionResult();
}
