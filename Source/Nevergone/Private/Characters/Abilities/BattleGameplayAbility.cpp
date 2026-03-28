// Copyright Xyzto Works


#include "Characters/Abilities/BattleGameplayAbility.h"

#include "ActorComponents/BattleModeComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/AbilityPreview/AbilityPreviewRenderer.h"
#include "Types/BattleTypes.h"

UBattleGameplayAbility::UBattleGameplayAbility()
{
	// Ability is instanced per actor to keep execution state isolated
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Ability must be explicitly activated by game logic
	bActivateAbilityOnGranted = false;
}

FActionResult UBattleGameplayAbility::BuildPreview(const FActionContext& Context) const
{
	FActionResult Result;
	Result.bIsValid = false;
	return Result;
}

FActionResult UBattleGameplayAbility::BuildExecution(const FActionContext& Context) const
{
	return BuildPreview(Context);
}

EBattleAbilitySelectionMode UBattleGameplayAbility::GetSelectionMode() const
{
	return EBattleAbilitySelectionMode::SingleConfirm;
}

void UBattleGameplayAbility::FinalizeAbilityExecution()
{
	ACharacterBase* Character = Cast<ACharacterBase>(GetAvatarActorFromActorInfo());
	UBattleModeComponent* BattleMode = Character ? Character->GetBattleModeComponent() : nullptr;

	ApplyAbilityCompletionEffects();

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

	if (BattleMode)
	{
		BattleMode->HandleActionFinished();
	}
}

void UBattleGameplayAbility::ApplyAbilityCompletionEffects()
{
	// Default: do nothing
}

void UBattleGameplayAbility::CleanupAbilityDelegates()
{
}

TSubclassOf<UAbilityPreviewRenderer> UBattleGameplayAbility::GetPreviewClass() const
{
	return PreviewRendererClass;
}
