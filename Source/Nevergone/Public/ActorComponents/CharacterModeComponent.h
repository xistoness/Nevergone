// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterModeComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class NEVERGONE_API UCharacterModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCharacterModeComponent();
	
	virtual void EnterMode() {}
	virtual void ExitMode() {}
		
};
