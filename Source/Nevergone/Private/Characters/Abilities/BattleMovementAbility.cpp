// Copyright Xyzto Works

#include "Characters/Abilities/BattleMovementAbility.h"

#include "Characters/CharacterBase.h"

FActionResult UBattleMovementAbility::BuildPreview(const FActionContext& Context) const
{
	return BuildMovementPreview(Context);
}

FActionResult UBattleMovementAbility::BuildExecution(const FActionContext& Context) const
{
	return BuildMovementExecution(Context);
}

FActionResult UBattleMovementAbility::BuildMovementPreview(const FActionContext& Context) const
{
	return Super::BuildPreview(Context);
}

FActionResult UBattleMovementAbility::BuildMovementExecution(const FActionContext& Context) const
{
	return BuildMovementPreview(Context);
}

bool UBattleMovementAbility::ExecuteMovementPlan(
	ACharacterBase* Character,
	const FActionResult& MovementResult,
	const FSimpleDelegate& OnMovementFinished
)
{
	return false;
}