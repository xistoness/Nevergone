// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "BattleMovementAbility.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class NEVERGONE_API UBattleMovementAbility : public UBattleGameplayAbility
{
	GENERATED_BODY()
public:
	virtual FActionResult BuildPreview(const FActionContext& Context) const override;
	virtual FActionResult BuildExecution(const FActionContext& Context) const override;

	virtual FActionResult BuildMovementPreview(const FActionContext& Context) const;
	virtual FActionResult BuildMovementExecution(const FActionContext& Context) const;

	virtual bool ExecuteMovementPlan(
		ACharacterBase* Character,
		const FActionResult& MovementResult,
		const FSimpleDelegate& OnMovementFinished
	);
};
