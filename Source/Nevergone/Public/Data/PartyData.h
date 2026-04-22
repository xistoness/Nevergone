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

	// Battle unit BP (APlayerUnitBase subclass) — used by the combat system
	// and to resolve display name / stats via CDO.
	UPROPERTY()
	TSubclassOf<ACharacter> CharacterClass;

	// Exploration pawn BP (ACharacterBase subclass) — the actor spawned in
	// the world when this member is the party leader.
	// Leave null to fall back to TowerFloorGameMode::ExplorationCharacterClass
	// (prototype default).
	UPROPERTY()
	TSubclassOf<ACharacter> ExplorationPawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level = 1;

	UPROPERTY()
	float CurrentHP = 0.f;

	UPROPERTY()
	float MaxHP = 0.f;

	UPROPERTY()
	bool bIsAlive = true;

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

	// CharacterID of the current party leader.
	// The leader is the exploration pawn and the first member in combat.
	// Kept in sync with Members[0] by SetPartyLeader().
	UPROPERTY()
	FGuid LeaderID;
};