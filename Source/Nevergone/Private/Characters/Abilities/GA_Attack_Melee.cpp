// Copyright Xyzto Works


#include "Characters/Abilities/GA_Attack_Melee.h"
#include "Types/BattleTypes.h"

UGA_Attack_Melee::UGA_Attack_Melee()
{
	// ===== Ability Asset Tags =====
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(
		FGameplayTag::RequestGameplayTag(
			FName("Ability.Attack.Melee")
		)
	);

	SetAssetTags(AssetTags);

	// ===== Needed Tags for activation =====
	ActivationRequiredTags.AddTag(
		FGameplayTag::RequestGameplayTag(
			FName("Mode.Combat")
		)
	);

	ActivationRequiredTags.AddTag(
		FGameplayTag::RequestGameplayTag(
			FName("Turn.CanAct")
		)
	);

	// ===== Tags that block activation =====
	ActivationBlockedTags.AddTag(
		FGameplayTag::RequestGameplayTag(
			FName("State.Stunned")
		)
	);
}

void UGA_Attack_Melee::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	// Validate event data
	if (!TriggerEventData)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Validate target data
	if (TriggerEventData->TargetData.Num() == 0)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Validate damage effect
	if (!DamageEffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Apply damage effect to targets provided by the event
	ApplyGameplayEffectToTarget(
		Handle,
		ActorInfo,
		ActivationInfo,
		TriggerEventData->TargetData,
		DamageEffectClass,
		GetAbilityLevel()
	);

	// Finish ability successfully
	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
