// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/PartyData.h"
#include "Types/BattleTypes.h"
#include "PartyManagerSubsystem.generated.h"

class UFloorEncounterData;

UCLASS()
class NEVERGONE_API UPartyManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	/* --- Party state --- */
	const FPartyData& GetPartyData() const;
	
	// --- Party mutation (temporary / debug / future save integration) ---
	void AddPartyMember(const FPartyMemberData& NewMember);
	void ClearParty();

	/* --- Battle integration --- */
	void BuildBattleParty(
		const UFloorEncounterData* EncounterData,
		TArray<FGeneratedPlayerData>& OutPlayerParty
	) const;
	
	// Called by GameContextManager after combat ends, before actors are destroyed.
	// Reads PersistentHP from each ally's UnitStatsComponent and updates PartyData.
	// AliveActorsBySourceIndex maps each alive ally's SourceIndex to its actor.
	// GeneratedParty[i] entered this combat as SourceIndex=i; members absent
	// from the map died during combat and are written back with HP=0.
	void WriteBackBattleResults(const TMap<int32, ACharacterBase*>& AliveActorsBySourceIndex,
								 const TArray<FGeneratedPlayerData>& GeneratedParty);
	
	// Called once after GameInstance loads a save slot, and after every level load.
	// Pulls the authoritative PartyData from GameInstance into this subsystem.
	void SyncFromGameInstance();

	// Called after WriteBackBattleResults to push updated data back to GameInstance
	// so CommitSave picks it up correctly.
	void FlushToGameInstance();

protected:
	/* --- Internal helpers --- */
	void AddBasePartyMembers(
		TArray<FGeneratedPlayerData>& OutParty
	) const;

	void AddEncounterGuests(
		const UFloorEncounterData* EncounterData,
		TArray<FGeneratedPlayerData>& OutParty
	) const;

private:
	UPROPERTY()
	FPartyData PartyData;
};