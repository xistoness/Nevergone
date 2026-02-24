// Copyright Xyzto Works


#include "Party/PartyManagerSubsystem.h"
#include "Characters/CharacterBase.h"
#include "Level/FloorEncounterData.h"

const FPartyData& UPartyManagerSubsystem::GetPartyData() const
{
	return PartyData;
}

void UPartyManagerSubsystem::AddPartyMember(const FPartyMemberData& NewMember)
{
	UE_LOG(LogTemp, Warning, TEXT("[PartyManagerSubsystem]: Add party member called: %s"), *NewMember.CharacterClass->GetName());
	PartyData.Members.Add(NewMember);
}

void UPartyManagerSubsystem::ClearParty()
{
	PartyData.Members.Reset();
}

void UPartyManagerSubsystem::BuildBattleParty(
	const UFloorEncounterData* EncounterData,
	TArray<FGeneratedPlayerData>& OutPlayerParty
) const
{
	OutPlayerParty.Reset();

	// 1. Base party (persistent members)
	AddBasePartyMembers(OutPlayerParty);

	// 2. Encounter-specific guests / overrides
	if (EncounterData)
	{
		AddEncounterGuests(EncounterData, OutPlayerParty);
	}
}

void UPartyManagerSubsystem::AddBasePartyMembers(
	TArray<FGeneratedPlayerData>& OutParty
) const
{
	for (const FPartyMemberData& Entry : PartyData.Members)
	{
		if (!Entry.bIsAlive)
			continue;

		FGeneratedPlayerData Generated;
		Generated.PlayerClass = Entry.CharacterClass;
		Generated.Level = Entry.Level;
		
		UE_LOG(LogTemp, Warning, TEXT("[PartyManagerSubsystem]: Base Party Member: %s"), *Generated.PlayerClass->GetName());

		// Optional: translate stats → tags later
		OutParty.Add(Generated);
	}
}

void UPartyManagerSubsystem::AddEncounterGuests(
	const UFloorEncounterData* EncounterData,
	TArray<FGeneratedPlayerData>& OutParty
) const
{
	for (const FAllyEntry& Ally : EncounterData->PossibleAllies)
	{
		FGeneratedPlayerData Guest;
		Guest.PlayerClass = Ally.PlayerClass;
		Guest.Tags = Ally.Tags;
		Guest.Level = Ally.Level;
		
		UE_LOG(LogTemp, Warning, TEXT("[PartyManagerSubsystem]: Guest Ally Member: %s"), *Guest.PlayerClass->GetName());

		OutParty.Add(Guest);
	}
}