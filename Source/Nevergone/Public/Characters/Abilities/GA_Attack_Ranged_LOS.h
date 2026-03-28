// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "TargetingRules/CompositeTargetingPolicy.h"
#include "GA_Attack_Ranged_LOS.generated.h"


class ACharacterBase;

UCLASS()
class NEVERGONE_API UGA_Attack_Ranged_LOS : public UBattleGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Attack_Ranged_LOS();

	virtual FActionResult BuildPreview(const FActionContext& Context) const override;
	virtual FActionResult BuildExecution(const FActionContext& Context) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void ApplyAbilityCompletionEffects() override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

protected:
	bool BuildAttackResult(const FActionContext& Context, FActionResult& OutResult) const;
	void ApplyResolvedAttack();
	void FinalizeAttackAction();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Battle")
	int32 MaxRange = 10;

	UPROPERTY(EditDefaultsOnly, Category = "Battle")
	float BaseDamageMultiplier = 1.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	int32 BaseActionPointsCost = 1;

private:
	UPROPERTY()
	TObjectPtr<ACharacterBase> CachedCharacter = nullptr;

	UPROPERTY()
	TObjectPtr<ACharacterBase> CachedTargetCharacter = nullptr;

	int32 CachedActionPointsCost = 0;
	
	CompositeTargetingPolicy TargetPolicy;
};
