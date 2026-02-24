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
	
	UPROPERTY()
	TSubclassOf<ACharacter> CharacterClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level;
	
	UPROPERTY()
	float CurrentHP;

	UPROPERTY()
	float MaxHP;

	UPROPERTY()
	bool bIsAlive;

	UPROPERTY()
	TMap<FName, int32> Stats;

	UPROPERTY()
	TArray<FName> EquippedItems;
};

USTRUCT(BlueprintType)
struct FPartyData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FPartyMemberData> Members;
	
	UPROPERTY()
	int32 MaxPartySize = 4;
};
