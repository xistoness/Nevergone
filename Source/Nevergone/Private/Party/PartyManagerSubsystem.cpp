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

void UPartyManagerSubsystem::WriteBackBattleResults(
	const TMap<int32, ACharacterBase*>& AliveActorsBySourceIndex,
	const TArray<FGeneratedPlayerData>& GeneratedParty)
{
	// Iterate GeneratedParty — the authoritative list of every member that
	// entered this combat. GeneratedParty[i] was spawned as SourceIndex=i.
	// Members absent from AliveActorsBySourceIndex died during combat (or
	// failed to spawn) and must be written back with HP=0.
	for (int32 i = 0; i < GeneratedParty.Num(); ++i)
	{
		const FGeneratedPlayerData& Generated = GeneratedParty[i];
		const FGuid& ID = Generated.CharacterID;
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

		ACharacterBase* const* AllyPtr = AliveActorsBySourceIndex.Find(i);
		ACharacterBase* Ally = (AllyPtr && IsValid(*AllyPtr)) ? *AllyPtr : nullptr;

		if (Ally)
		{
			if (UUnitStatsComponent* Stats = Ally->GetUnitStats())
			{
				// PersistentHP was already written by BattleState::PersistToCombatants
				// before Cleanup destroys the actors — read it here while the actor lives.
				Member->CurrentHP = static_cast<float>(Stats->GetCurrentHP());
				Member->bIsAlive  = Stats->IsAlive();

				UE_LOG(LogTemp, Log,
					TEXT("[PartyManagerSubsystem] WriteBack: %s (SourceIndex=%d) | HP=%.0f | Alive=%s"),
					*GetNameSafe(Ally), i, Member->CurrentHP,
					Member->bIsAlive ? TEXT("YES") : TEXT("NO"));
			}
		}
		else
		{
			// Unit absent from SpawnedAllies — died during combat, failed to
			// spawn, or was absent on a restore path. Mark dead unconditionally.
			Member->CurrentHP = 0.f;
			Member->bIsAlive  = false;

			UE_LOG(LogTemp, Log,
				TEXT("[PartyManagerSubsystem] WriteBack: SourceIndex=%d (ID=%s) not alive — marked dead"),
				i, *ID.ToString());
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