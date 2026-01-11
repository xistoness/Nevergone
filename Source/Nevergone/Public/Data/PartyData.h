// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "PartyData.generated.h"



USTRUCT(BlueprintType)
struct FPartyMemberData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid CharacterID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level;
};

USTRUCT(BlueprintType)
struct FPartyData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FPartyMemberData> Members;
};
