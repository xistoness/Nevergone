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
