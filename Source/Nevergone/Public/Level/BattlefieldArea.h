// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "BattlefieldArea.generated.h"

UCLASS()
class NEVERGONE_API ABattlefieldArea : public AActor
{
	GENERATED_BODY()

public:
	ABattlefieldArea();

protected:
	UPROPERTY(EditInstanceOnly)
	TArray<FTransform> PlayerSpawnSlots;

	UPROPERTY(EditInstanceOnly)
	TArray<FTransform> EnemySpawnSlots;
	
	UPROPERTY(EditDefaultsOnly)
	FGameplayTagContainer BattlefieldTags;
	
	UPROPERTY(EditDefaultsOnly)
	FGameplayTagContainer TerrainTags;
};
