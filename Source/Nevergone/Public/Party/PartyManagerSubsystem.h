// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/PartyData.h"
#include "Types/BattleTypes.h"
#include "PartyManagerSubsystem.generated.h"

class UFloorEncounterData;

DECLARE_MULTICAST_DELEGATE(FOnPartyRosterChanged);

UCLASS()
class NEVERGONE_API UPartyManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    /* --- Party state --- */
    const FPartyData& GetPartyData() const;

    // Returns only members flagged as active (battle party, max 4).
    TArray<FPartyMemberData*> GetActiveMembers();

    // Returns all members NOT flagged as active (bench/roster).
    TArray<FPartyMemberData*> GetBenchMembers();

    // Returns the number of currently active party members.
    int32 GetActiveMemberCount() const;

    /* --- Party mutation --- */
    void AddPartyMember(const FPartyMemberData& NewMember);
    void ClearParty();

    // Sets a member's active flag by CharacterID.
    // Enforces MaxPartySize — returns false if the party is already full
    // and the member is not already active.
    bool SetMemberActive(const FGuid& CharacterID, bool bActive);

    // Swaps the active state between two members by CharacterID.
    // Used by the Party Manager UI when replacing an active slot with a bench member.
    bool SwapActiveMembers(const FGuid& IncomingID, const FGuid& OutgoingID);

    /* --- Battle integration --- */
    void BuildBattleParty(
        const UFloorEncounterData* EncounterData,
        TArray<FGeneratedPlayerData>& OutPlayerParty
    ) const;

    void WriteBackBattleResults(const TMap<int32, ACharacterBase*>& AliveActorsBySourceIndex,
                                const TArray<FGeneratedPlayerData>& GeneratedParty);

    void SyncFromGameInstance();
    void FlushToGameInstance();

    // Fired whenever the active/bench composition changes.
    // UI widgets bind to this to refresh without polling.
    FOnPartyRosterChanged OnRosterChanged;

protected:
    void AddBasePartyMembers(TArray<FGeneratedPlayerData>& OutParty) const;
    void AddEncounterGuests(const UFloorEncounterData* EncounterData,
                            TArray<FGeneratedPlayerData>& OutParty) const;

private:
    UPROPERTY()
    FPartyData PartyData;

    // Resolves MaxHP from the CharacterClass CDO using the UnitDefinition formula,
    // and fills CurrentHP to MaxHP if the member has never been in combat.
    // Called by AddPartyMember to guarantee HP fields are never zero.
    void ResolveInitialHP(FPartyMemberData& InOutMember) const;;
};