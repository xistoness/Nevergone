// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ExplorationInputReceiver.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UExplorationInputReceiver : public UInterface
{
	GENERATED_BODY()
};

class NEVERGONE_API IExplorationInputReceiver
{
	GENERATED_BODY()

public:
	virtual void Input_Interact() = 0;
	virtual void Input_Save() = 0;
	virtual void Input_Load() = 0;
};
