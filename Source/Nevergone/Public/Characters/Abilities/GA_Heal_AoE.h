// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "TargetingRules/CompositeTargetingPolicy.h"
#include "GA_Heal_AoE.generated.h"

class ACharacterBase;

UCLASS()
class NEVERGONE_API UGA_Heal_AoE : public UBattleGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Heal_AoE();

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
	bool BuildHealResult(const FActionContext& Context, FActionResult& OutResult) const;
	bool GatherAffectedAllies(
		ACharacterBase* SourceCharacter,
		const FIntPoint& CenterCoord,
		TArray<ACharacterBase*>& OutAllies
	) const;

	void ApplyResolvedHeal();
	void FinalizeHealAction();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Battle")
	int32 CastRange = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Battle")
	int32 HealRadius = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Battle")
	float HealAmount = 25.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	int32 BaseActionPointsCost = 1;

private:
	UPROPERTY()
	TObjectPtr<ACharacterBase> CachedCharacter = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<ACharacterBase>> CachedAffectedAllies;

	int32 CachedActionPointsCost = 0;
	FIntPoint CachedCenterCoord = FIntPoint::ZeroValue;
	
	CompositeTargetingPolicy TargetPolicy;
};