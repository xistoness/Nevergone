// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/CharacterBase.h"
#include "EnemyUnitBase.generated.h"

class UBattleAIComponent;

UCLASS()
class NEVERGONE_API AEnemyUnitBase : public ACharacterBase
{
	GENERATED_BODY()
	
public:
	
	AEnemyUnitBase();
	
protected:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI")
	UBattleAIComponent* BattleAIComponent = nullptr;
};
