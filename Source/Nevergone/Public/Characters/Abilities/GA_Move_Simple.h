// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "TargetingRules/CompositeTargetingPolicy.h"

#include "GA_Move_Simple.generated.h"

/**
 * 
 */
UCLASS()
class NEVERGONE_API UGA_Move_Simple : public UBattleGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_Move_Simple();
	
	virtual FActionResult BuildPreview(const FActionContext& Context) const override;

	// Maximum tiles this unit can move
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Move")
	int32 MaxMoveRange = 5;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;


private:
	CompositeTargetingPolicy TargetPolicy;
};
