// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "TargetingRules/CompositeTargetingPolicy.h"
#include "GA_Attack_Melee.generated.h"

class ACharacterBase;
class UBattleMovementAbility;

UCLASS()
class NEVERGONE_API UGA_Attack_Melee : public UBattleGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Attack_Melee();
	virtual EBattleAbilitySelectionMode GetSelectionMode() const override;

	virtual FActionResult BuildPreview(const FActionContext& Context) const override;
	virtual FActionResult BuildExecution(const FActionContext& Context) const override;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

	UFUNCTION()
	virtual void ApplyAbilityCompletionEffects() override;

	UFUNCTION()
	void HandleApproachMovementFinished();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	int32 MaxRange = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	int32 BaseActionPointsCost = 1;

protected:
	UPROPERTY()
	ACharacterBase* CachedCharacter = nullptr;

	UPROPERTY()
	ACharacterBase* CachedTargetCharacter = nullptr;

	UPROPERTY()
	int32 CachedActionPointsCost = 0;

	UPROPERTY()
	bool bCachedRequiredMovement = false;

	UPROPERTY()
	FIntPoint CachedFinalGridCoord;

	UPROPERTY()
	UBattleMovementAbility* ActiveMovementExecutor = nullptr;

private:
	bool BuildAttackResult(const FActionContext& Context, FActionResult& OutResult) const;
	
	bool TryBuildManualApproachResult(
		const FActionContext& Context,
		ACharacterBase* SourceCharacter,
		ACharacterBase* TargetCharacter,
		FActionResult& OutResult
	) const;
	
	bool TryBuildApproachResult(
		const FActionContext& Context,
		ACharacterBase* SourceCharacter,
		ACharacterBase* TargetCharacter,
		FActionResult& OutResult
	) const;

	bool IsTargetValidForMelee(
		ACharacterBase* SourceCharacter,
		ACharacterBase* TargetCharacter
	) const;

	bool IsAdjacent(const FIntPoint& Source, const FIntPoint& Target) const;

	TArray<FIntPoint> GetAdjacentCoords(const FIntPoint& Center) const;

	void ApplyResolvedAttack();
	void FinalizeAttackAction();

private:
	CompositeTargetingPolicy TargetPolicy;
};