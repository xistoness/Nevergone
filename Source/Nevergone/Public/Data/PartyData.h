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
	float CurrentHP = 0.f;

	UPROPERTY()
	float MaxHP = 0.f;

	UPROPERTY()
	bool bIsAlive;

	// Whether this member is in the active battle party (max 4).
	// Members not active remain in the roster but do not participate in combat.
	UPROPERTY()
	bool bIsActivePartyMember = false;

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