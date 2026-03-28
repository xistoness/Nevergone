// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Types/BattleTypes.h"
#include "BattleGameplayAbility.generated.h"

class UAbilityDefinition;
class UAbilityPreviewRenderer;

UCLASS(Abstract)
class NEVERGONE_API UBattleGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UBattleGameplayAbility();
	
	virtual FActionResult BuildPreview(const FActionContext& Context) const;
	virtual FActionResult BuildExecution(const FActionContext& Context) const;
	virtual EBattleAbilitySelectionMode GetSelectionMode() const;
	virtual void FinalizeAbilityExecution();
	virtual void ApplyAbilityCompletionEffects();
	
	void CleanupAbilityDelegates();
	
	virtual TSubclassOf<UAbilityPreviewRenderer> GetPreviewClass() const;
	
	// Definition that describes this ability (used by UI and AI)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAbilityDefinition* AbilityDefinition;
	
	bool bActivateAbilityOnGranted;
	
	UPROPERTY(EditDefaultsOnly, Category="Preview")
	TSubclassOf<UAbilityPreviewRenderer> PreviewRendererClass;
	
};
