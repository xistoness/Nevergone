// Copyright Xyzto Works

#include "Party/PartyManagerSubsystem.h"

#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/PlayerUnitBase.h"
#include "Data/UnitDefinition.h"
#include "GameInstance/MyGameInstance.h"
#include "Level/FloorEncounterData.h"

// ---------------------------------------------------------------------------
// Party state queries
// ---------------------------------------------------------------------------

const FPartyData& UPartyManagerSubsystem::GetPartyData() const
{
    return PartyData;
}

TArray<FPartyMemberData*> UPartyManagerSubsystem::GetActiveMembers()
{
    TArray<FPartyMemberData*> Result;
    for (FPartyMemberData& Member : PartyData.Members)
    {
        if (Member.bIsActivePartyMember)
        {
            Result.Add(&Member);
        }
    }
    return Result;
}

TArray<FPartyMemberData*> UPartyManagerSubsystem::GetBenchMembers()
{
    TArray<FPartyMemberData*> Result;
    for (FPartyMemberData& Member : PartyData.Members)
    {
        if (!Member.bIsActivePartyMember)
        {
            Result.Add(&Member);
        }
    }
    return Result;
}

int32 UPartyManagerSubsystem::GetActiveMemberCount() const
{
    int32 Count = 0;
    for (const FPartyMemberData& Member : PartyData.Members)
    {
        if (Member.bIsActivePartyMember) { ++Count; }
    }
    return Count;
}

// ---------------------------------------------------------------------------
// Party mutation
// ---------------------------------------------------------------------------

void UPartyManagerSubsystem::AddPartyMember(const FPartyMemberData& NewMember)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[PartyManagerSubsystem] AddPartyMember: %s"),
        *NewMember.CharacterClass->GetName());

    FPartyMemberData& Added = PartyData.Members.Add_GetRef(NewMember);

    ResolveInitialHP(Added);

    // Auto-activate if there is room in the active party
    if (GetActiveMemberCount() < PartyData.MaxPartySize)
    {
        Added.bIsActivePartyMember = true;

        UE_LOG(LogTemp, Log,
            TEXT("[PartyManagerSubsystem] AddPartyMember: auto-activated %s (%d/%d slots used)"),
            *Added.CharacterClass->GetName(),
            GetActiveMemberCount(), PartyData.MaxPartySize);

        // If there is no leader yet, the first activated member becomes leader.
        // This covers new game initialization where members are added sequentially
        // and no leader has been explicitly set.
        if (!PartyData.LeaderID.IsValid())
        {
            PartyData.LeaderID = Added.CharacterID;

            UE_LOG(LogTemp, Log,
                TEXT("[PartyManagerSubsystem] AddPartyMember: no leader set — '%s' becomes leader automatically."),
                *Added.CharacterClass->GetName());
        }
    }
    else
    {
        UE_LOG(LogTemp, Log,
            TEXT("[PartyManagerSubsystem] AddPartyMember: party full (%d/%d), %s added to bench"),
            GetActiveMemberCount(), PartyData.MaxPartySize,
            *Added.CharacterClass->GetName());
    }

    OnRosterChanged.Broadcast();
    FlushToGameInstance();
}

void UPartyManagerSubsystem::ResolveInitialHP(FPartyMemberData& InOutMember) const
{
    if (!InOutMember.CharacterClass) { return; }
    if (InOutMember.MaxHP > 0.f) { return; }

    APlayerUnitBase* CDO = InOutMember.CharacterClass->GetDefaultObject<APlayerUnitBase>();
    if (!CDO)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PartyManagerSubsystem] ResolveInitialHP: CDO is not APlayerUnitBase for %s."),
            *InOutMember.CharacterClass->GetName());
        return;
    }

    UUnitStatsComponent* StatsComp = CDO->GetUnitStats();
    if (!StatsComp || !StatsComp->GetDefinition())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PartyManagerSubsystem] ResolveInitialHP: missing StatsComp or Definition on CDO of %s."),
            *InOutMember.CharacterClass->GetName());
        return;
    }

    // Temporarily apply the member's level to the CDO so GetMaxHP() returns
    // the correct value. The CDO level is restored immediately after.
    // This avoids duplicating the MaxHP formula and guarantees parity with
    // whatever GetMaxHP() returns at combat init time.
    const int32 OriginalLevel = StatsComp->Level;
    StatsComp->Level = InOutMember.Level;
    const float DerivedMaxHP = static_cast<float>(StatsComp->GetMaxHP());
    StatsComp->Level = OriginalLevel;

    InOutMember.MaxHP = DerivedMaxHP;

    if (InOutMember.CurrentHP <= 0.f)
    {
        InOutMember.CurrentHP = DerivedMaxHP;
    }

    UE_LOG(LogTemp, Log,
        TEXT("[PartyManagerSubsystem] ResolveInitialHP: %s Lv%d -> MaxHP=%.0f, CurrentHP=%.0f"),
        *InOutMember.CharacterClass->GetName(),
        InOutMember.Level,
        InOutMember.MaxHP,
        InOutMember.CurrentHP);
}

void UPartyManagerSubsystem::ClearParty()
{
    PartyData.Members.Reset();
}

bool UPartyManagerSubsystem::SetMemberActive(const FGuid& CharacterID, bool bActive)
{
    FPartyMemberData* Target = PartyData.Members.FindByPredicate(
        [&CharacterID](const FPartyMemberData& M) { return M.CharacterID == CharacterID; }
    );

    if (!Target)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PartyManagerSubsystem] SetMemberActive: ID=%s not found."),
            *CharacterID.ToString());
        return false;
    }

    // Activating: check party is not already full
    if (bActive && !Target->bIsActivePartyMember)
    {
        if (GetActiveMemberCount() >= PartyData.MaxPartySize)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[PartyManagerSubsystem] SetMemberActive: party full (%d/%d), cannot activate ID=%s."),
                GetActiveMemberCount(), PartyData.MaxPartySize, *CharacterID.ToString());
            return false;
        }
    }

    Target->bIsActivePartyMember = bActive;

    UE_LOG(LogTemp, Log,
        TEXT("[PartyManagerSubsystem] SetMemberActive: ID=%s -> %s"),
        *CharacterID.ToString(), bActive ? TEXT("Active") : TEXT("Bench"));

    OnRosterChanged.Broadcast();
    FlushToGameInstance();
    return true;
}

bool UPartyManagerSubsystem::SwapActiveMembers(const FGuid& IncomingID, const FGuid& OutgoingID)
{
    // Incoming must currently be on bench, outgoing must currently be active
    FPartyMemberData* Incoming = PartyData.Members.FindByPredicate(
        [&IncomingID](const FPartyMemberData& M) { return M.CharacterID == IncomingID; }
    );
    FPartyMemberData* Outgoing = PartyData.Members.FindByPredicate(
        [&OutgoingID](const FPartyMemberData& M) { return M.CharacterID == OutgoingID; }
    );

    if (!Incoming || !Outgoing)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PartyManagerSubsystem] SwapActiveMembers: one or both IDs not found. In=%s Out=%s"),
            *IncomingID.ToString(), *OutgoingID.ToString());
        return false;
    }

    if (Incoming->bIsActivePartyMember)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PartyManagerSubsystem] SwapActiveMembers: incoming ID=%s is already active."),
            *IncomingID.ToString());
        return false;
    }

    if (!Outgoing->bIsActivePartyMember)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PartyManagerSubsystem] SwapActiveMembers: outgoing ID=%s is not active."),
            *OutgoingID.ToString());
        return false;
    }

    Outgoing->bIsActivePartyMember = false;
    Incoming->bIsActivePartyMember = true;

    UE_LOG(LogTemp, Log,
        TEXT("[PartyManagerSubsystem] SwapActiveMembers: %s (bench) <-> %s (active)"),
        *IncomingID.ToString(), *OutgoingID.ToString());

    OnRosterChanged.Broadcast();
    FlushToGameInstance();
    return true;
}

// ---------------------------------------------------------------------------
// Battle integration
// ---------------------------------------------------------------------------

void UPartyManagerSubsystem::BuildBattleParty(
    const UFloorEncounterData* EncounterData,
    TArray<FGeneratedPlayerData>& OutPlayerParty
) const
{
    OutPlayerParty.Reset();
    AddBasePartyMembers(OutPlayerParty);

    if (EncounterData)
    {
        AddEncounterGuests(EncounterData, OutPlayerParty);
    }
}

void UPartyManagerSubsystem::WriteBackBattleResults(
    const TMap<int32, ACharacterBase*>& AliveActorsBySourceIndex,
    const TArray<FGeneratedPlayerData>& GeneratedParty)
{
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
            Member->CurrentHP = 0.f;
            Member->bIsAlive  = false;

            UE_LOG(LogTemp, Log,
                TEXT("[PartyManagerSubsystem] WriteBack: SourceIndex=%d (ID=%s) not alive — marked dead"),
                i, *ID.ToString());
        }
    }

    FlushToGameInstance();
}

void UPartyManagerSubsystem::SyncFromGameInstance()
{
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) { return; }

    PartyData = GI->GetPartyData();

    UE_LOG(LogTemp, Log,
        TEXT("[PartyManagerSubsystem] SyncFromGameInstance: loaded %d members (%d active)"),
        PartyData.Members.Num(), GetActiveMemberCount());
}

void UPartyManagerSubsystem::FlushToGameInstance()
{
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) { return; }

    GI->SetPartyData(PartyData);

    UE_LOG(LogTemp, Log,
        TEXT("[PartyManagerSubsystem] FlushToGameInstance: pushed %d members (%d active)"),
        PartyData.Members.Num(), GetActiveMemberCount());
}

const FPartyMemberData* UPartyManagerSubsystem::GetPartyLeader() const
{
    // Leader is always Members[0] by convention.
    // Fall back to first active member if LeaderID is not set yet.
    if (PartyData.LeaderID.IsValid())
    {
        for (const FPartyMemberData& Member : PartyData.Members)
        {
            if (Member.CharacterID == PartyData.LeaderID)
            {
                return &Member;
            }
        }
    }

    // Fallback: first active member
    for (const FPartyMemberData& Member : PartyData.Members)
    {
        if (Member.bIsActivePartyMember) { return &Member; }
    }

    return nullptr;
}

bool UPartyManagerSubsystem::SetPartyLeader(const FGuid& NewLeaderID)
{
    // Find the member
    int32 LeaderIndex = INDEX_NONE;
    for (int32 i = 0; i < PartyData.Members.Num(); ++i)
    {
        if (PartyData.Members[i].CharacterID == NewLeaderID)
        {
            LeaderIndex = i;
            break;
        }
    }

    if (LeaderIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PartyManagerSubsystem] SetPartyLeader: ID=%s not found."),
            *NewLeaderID.ToString());
        return false;
    }

    if (!PartyData.Members[LeaderIndex].bIsActivePartyMember)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PartyManagerSubsystem] SetPartyLeader: ID=%s is not an active member."),
            *NewLeaderID.ToString());
        return false;
    }

    if (PartyData.LeaderID == NewLeaderID)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[PartyManagerSubsystem] SetPartyLeader: ID=%s is already the leader."),
            *NewLeaderID.ToString());
        return true;
    }

    // Move the new leader to index 0 so BuildBattleParty, BuildSaveDisplayName,
    // and GameContextManager always find them at Members[0].
    PartyData.Members.Swap(0, LeaderIndex);
    PartyData.LeaderID = NewLeaderID;

    UE_LOG(LogTemp, Log,
        TEXT("[PartyManagerSubsystem] SetPartyLeader: new leader ID=%s"),
        *NewLeaderID.ToString());

    OnRosterChanged.Broadcast();
    OnLeaderChanged.Broadcast();
    FlushToGameInstance();

    return true;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void UPartyManagerSubsystem::AddBasePartyMembers(TArray<FGeneratedPlayerData>& OutParty) const
{
    // Only active members enter combat — bench members are excluded.
    for (const FPartyMemberData& Entry : PartyData.Members)
    {
        if (!Entry.bIsActivePartyMember) { continue; }
        if (!Entry.bIsAlive) { continue; }

        FGeneratedPlayerData Generated;
        Generated.PlayerClass  = Entry.CharacterClass;
        Generated.Level        = Entry.Level;
        Generated.PersistentHP = Entry.CurrentHP > 0 ? FMath::RoundToInt(Entry.CurrentHP) : 0;
        Generated.CharacterID  = Entry.CharacterID;

        UE_LOG(LogTemp, Log,
            TEXT("[PartyManagerSubsystem] AddBasePartyMembers: %s | HP=%d | ID=%s"),
            *GetNameSafe(Generated.PlayerClass),
            Generated.PersistentHP,
            *Generated.CharacterID.ToString());

        OutParty.Add(Generated);
    }
}

void UPartyManagerSubsystem::AddEncounterGuests(
    const UFloorEncounterData* EncounterData,
    TArray<FGeneratedPlayerData>& OutParty) const
{
    for (const FAllyEntry& Ally : EncounterData->PossibleAllies)
    {
        FGeneratedPlayerData Guest;
        Guest.PlayerClass = Ally.PlayerClass;
        Guest.Tags        = Ally.Tags;
        Guest.Level       = Ally.Level;

        UE_LOG(LogTemp, Log,
            TEXT("[PartyManagerSubsystem] AddEncounterGuests: %s"),
            *Guest.PlayerClass->GetName());

        OutParty.Add(Guest);
    }
}