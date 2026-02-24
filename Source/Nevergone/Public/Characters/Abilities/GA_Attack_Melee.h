// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "GA_Attack_Melee.generated.h"


UCLASS()
class NEVERGONE_API UGA_Attack_Melee : public UBattleGameplayAbility
{
	GENERATED_BODY()
public:
	
	UGA_Attack_Melee();
	

protected:
	
	// GameplayEffect applied as damage
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, 
		const FGameplayEventData* TriggerEventData
		) override;

};
