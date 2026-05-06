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

    // Mutable view of the runtime PartyData cache. Use only for mutations
    // that don't fit the FindMemberMutable / NotifyMemberMutated pattern
    // (currently: top-level fields like Gold, MaxPartySize). Caller is
    // responsible for invoking NotifyMemberMutated() afterwards so the
    // GameInstance copy and subscribed UI stay in sync.
    FPartyData& GetPartyDataMutable() { return PartyData; }

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
    );

    void WriteBackBattleResults(const TMap<int32, ACharacterBase*>& AliveActorsBySourceIndex,
                                const TArray<FGeneratedPlayerData>& GeneratedParty);

    void SyncFromGameInstance();
    void FlushToGameInstance();

    // Returns the current party leader data, or nullptr if the party is empty.
    const FPartyMemberData* GetPartyLeader() const;

    // -----------------------------------------------------------------------
    // Runtime mutation access (single owner of the in-memory FPartyData cache)
    // -----------------------------------------------------------------------

    /**
     * Returns a mutable pointer to the FPartyMemberData with the given ID,
     * resolved from the subsystem's runtime cache (NOT directly from the
     * GameInstance copy). Returns nullptr if no member matches.
     *
     * Use this from any system that needs to mutate persistent member fields
     * (HP, alive flag, equipment) at runtime — e.g. UItemUsageSubsystem,
     * UInventorySubsystem. After mutating, call NotifyMemberMutated() to
     * propagate the change back to the GameInstance and refresh the UI.
     */
    FPartyMemberData* FindMemberMutable(const FGuid& CharacterID);

    /**
     * Flushes the runtime cache to the GameInstance and broadcasts
     * OnRosterChanged so any subscribed UI (PartyManagerWidget, HUD, etc.)
     * refreshes immediately. Call after every mutation made via
     * FindMemberMutable() or GetActiveMembers() pointers.
     */
    void NotifyMemberMutated();

    // Sets the member with the given ID as party leader.
    // Moves them to index 0 in Members and updates FPartyData::LeaderID.
    // Fires OnRosterChanged and OnLeaderChanged.
    // Returns false if the ID is not found or is not an active member.
    bool SetPartyLeader(const FGuid& NewLeaderID);

    // Fired when the leader changes — GameContextManager binds to this
    // to swap the exploration pawn immediately.
    FOnPartyRosterChanged OnLeaderChanged;    
    
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
    void ResolveInitialHP(FPartyMemberData& InOutMember) const;
    void ResolveDisplayName(FPartyMemberData& InOutMember) const;
};