// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GridAware.generated.h"

class UGridManager;

UINTERFACE(MinimalAPI)
class UGridAware : public UInterface
{
	GENERATED_BODY()
};


class NEVERGONE_API IGridAware
{
	GENERATED_BODY()


public:
	virtual void SetGridManager(UGridManager* InGridManager) = 0;

};
