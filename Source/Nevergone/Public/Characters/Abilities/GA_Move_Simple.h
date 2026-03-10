// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/BattleMovementAbility.h"
#include "TargetingRules/CompositeTargetingPolicy.h"
#include "GA_Move_Simple.generated.h"

class ACharacterBase;

UCLASS()
class NEVERGONE_API UGA_Move_Simple : public UBattleMovementAbility
{
	GENERATED_BODY()

public:
	UGA_Move_Simple();

	virtual FActionResult BuildMovementPreview(const FActionContext& Context) const override;
	virtual FActionResult BuildMovementExecution(const FActionContext& Context) const override;

	virtual bool ExecuteMovementPlan(
		ACharacterBase* Character,
		const FActionResult& MovementResult,
		const FSimpleDelegate& OnMovementFinished
	) override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

protected:
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

	UFUNCTION()
	void ApplyAbilityCompletionEffects() override;
	
	UFUNCTION()
	void HandleExternalMovementFinished();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Move")
	int32 MaxMoveRange = 5;

	UPROPERTY()
	ACharacterBase* CachedCharacter = nullptr;
	
	UPROPERTY()
	int32 CachedActionPointsCost = 0;
	
	UPROPERTY()
	FIntPoint CachedDestinationGridCoord;

private:
	CompositeTargetingPolicy TargetPolicy;
	FSimpleDelegate PendingMovementFinishedDelegate;
	FDelegateHandle MovementFinishedDelegateHandle;	
};