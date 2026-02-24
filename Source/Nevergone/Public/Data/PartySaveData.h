// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Data/PartyData.h"
#include "PartySaveData.generated.h"


USTRUCT(BlueprintType)
struct FPartySaveData
{
	GENERATED_BODY()

public:
	
	UPROPERTY()
	FPartyData PartyData;
};
