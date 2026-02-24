// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "AbilityDefinition.generated.h"


UCLASS()
class NEVERGONE_API UAbilityDefinition : public UDataAsset
{
	GENERATED_BODY()
	
public:
	// UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UTexture2D* Icon;

	// Classification
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag AbilityTag;

	// Range / targeting (dados puros)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 MinRange = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 MaxRange = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bRequiresLineOfSight = false;
	
};
