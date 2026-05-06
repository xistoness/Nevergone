// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/ItemTypes.h"
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

	// Display name resolved from UnitDefinition::DisplayName when the member
	// is added via AddPartyMember. Read by UI systems without needing the CDO.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;

	// Class name resolved from UnitDefinition::GetClassName() when added.
	// Kept in sync with DisplayName so callers never need the CDO for labels.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText ClassName;
	
	// Canonical unit class enum resolved from UnitDefinition when the member
	// is added. Used by InventorySubsystem for equipment class restriction checks
	// without requiring the character actor to be alive in the world.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EUnitClass UnitClass = EUnitClass::None;

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

	// Per-character equipment: four gear slots + four combat consumable slots.
	// Replaces the old TArray<FName> EquippedItems for type-safety and slot clarity.
	UPROPERTY()
	FEquipmentLoadout EquipmentLoadout;
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

	// Shared party currency. Currency items (UItemDefinition with
	// Category == EItemCategory::Currency) are intercepted by the inventory
	// subsystem on pickup / transfer and added to this counter instead of
	// occupying a slot. Read-only to BP — write through UInventorySubsystem
	// AddGold / SpendGold so the OnGoldChanged delegate fires.
	UPROPERTY(BlueprintReadOnly)
	int32 Gold = 0;
};