// Copyright Xyzto Works


#include "Party/PartyManagerSubsystem.h"

#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "GameInstance/MyGameInstance.h"
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

void UPartyManagerSubsystem::WriteBackBattleResults(const TArray<ACharacterBase*>& SpawnedAllies,
	const TArray<FGeneratedPlayerData>& GeneratedParty)
{
	// SpawnedAllies[i] corresponds to GeneratedParty[i] — same spawn order.
	// Use CharacterID from GeneratedParty to find the matching FPartyMemberData entry.
	for (int32 i = 0; i < SpawnedAllies.Num() && i < GeneratedParty.Num(); ++i)
	{
		ACharacterBase* Ally = SpawnedAllies[i];
		const FGuid& ID = GeneratedParty[i].CharacterID;

		if (!ID.IsValid()) { continue; }

		FPartyMemberData* Member = PartyData.Members.FindByPredicate(
			[&ID](const FPartyMemberData& M) { return M.CharacterID == ID; }
		);

		if (!Member)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[PartyManagerSubsystem] WriteBack: no party member found for ID=%s"),
				*ID.ToString());
			continue;
		}

		if (IsValid(Ally))
		{
			if (UUnitStatsComponent* Stats = Ally->GetUnitStats())
			{
				// PersistentHP was already written by BattleState::PersistToCombatants
				// before Cleanup destroys the actors — read it here while the actor lives.
				Member->CurrentHP = static_cast<float>(Stats->GetCurrentHP());
				Member->bIsAlive  = Stats->IsAlive();

				UE_LOG(LogTemp, Log,
					TEXT("[PartyManagerSubsystem] WriteBack: %s | HP=%.0f | Alive=%s"),
					*GetNameSafe(Ally),
					Member->CurrentHP,
					Member->bIsAlive ? TEXT("YES") : TEXT("NO"));
			}
		}
		else
		{
			// Actor was already destroyed — shouldn't happen if called before Cleanup,
			// but handle gracefully.
			UE_LOG(LogTemp, Warning,
				TEXT("[PartyManagerSubsystem] WriteBack: ally actor invalid for ID=%s — marking dead"),
				*ID.ToString());
			Member->CurrentHP = 0.f;
			Member->bIsAlive  = false;
		}
	}
	// Push the updated data to GameInstance so CommitSave picks it up.
	FlushToGameInstance();
}

void UPartyManagerSubsystem::SyncFromGameInstance()
{
	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	if (!GI) { return; }

	PartyData = GI->GetPartyData();

	UE_LOG(LogTemp, Log,
		TEXT("[PartyManagerSubsystem] SyncFromGameInstance: loaded %d members from GameInstance"),
		PartyData.Members.Num());
}

void UPartyManagerSubsystem::FlushToGameInstance()
{
	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	if (!GI) { return; }

	GI->SetPartyData(PartyData);

	UE_LOG(LogTemp, Log,
		TEXT("[PartyManagerSubsystem] FlushToGameInstance: pushed %d members to GameInstance"),
		PartyData.Members.Num());
}

void UPartyManagerSubsystem::AddBasePartyMembers(
	TArray<FGeneratedPlayerData>& OutParty
) const
{
	for (const FPartyMemberData& Entry : PartyData.Members)
	{
		if (!Entry.bIsAlive) { continue; }

		FGeneratedPlayerData Generated;
		Generated.PlayerClass  = Entry.CharacterClass;
		Generated.Level        = Entry.Level;
		Generated.PersistentHP = Entry.CurrentHP > 0 ? FMath::RoundToInt(Entry.CurrentHP) : 0;
		Generated.CharacterID  = Entry.CharacterID;

		UE_LOG(LogTemp, Warning,
			TEXT("[PartyManagerSubsystem] Base party member: %s | HP=%d | ID=%s"),
			*GetNameSafe(Generated.PlayerClass),
			Generated.PersistentHP,
			*Generated.CharacterID.ToString());

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