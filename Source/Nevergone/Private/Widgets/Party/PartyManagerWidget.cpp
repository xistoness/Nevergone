// Copyright Xyzto Works

#include "Widgets/Party/PartyManagerWidget.h"

#include "Party/PartyManagerSubsystem.h"
#include "Widgets/Party/PartyMemberEntryWidget.h"
#include "Data/PartyData.h"

void UPartyManagerWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (UGameInstance* GI = GetGameInstance())
    {
        PartySubsystem = GI->GetSubsystem<UPartyManagerSubsystem>();
    }

    if (!PartySubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[PartyManagerWidget] PartyManagerSubsystem not found."));
        return;
    }

    PartySubsystem->OnRosterChanged.RemoveAll(this);
    PartySubsystem->OnRosterChanged.AddUObject(this, &UPartyManagerWidget::RefreshPartyManager);

    RefreshPartyManager();

    UE_LOG(LogTemp, Log, TEXT("[PartyManagerWidget] Constructed."));
}

void UPartyManagerWidget::NativeDestruct()
{
    if (PartySubsystem)
    {
        PartySubsystem->OnRosterChanged.RemoveAll(this);
    }

    Super::NativeDestruct();
}

void UPartyManagerWidget::RefreshPartyManager()
{
    if (!PartySubsystem) { return; }
    if (!MemberEntryWidgetClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[PartyManagerWidget] MemberEntryWidgetClass not set."));
        return;
    }

    BP_ClearEntries();
    AllEntries.Reset();
    ActiveEntryCount = 0;

    // Reset all pending state on full rebuild
    PendingSwapIncomingID.Invalidate();
    PendingActiveID.Invalidate();
    ActiveConfirmState = EActiveConfirmState::None;
    BP_SetSwapPendingState(false, FText::GetEmpty());

    const FPartyData& Data         = PartySubsystem->GetPartyData();
    const FGuid       LeaderID     = Data.LeaderID;

    // Col 0: active members
    for (const FPartyMemberData& Member : Data.Members)
    {
        if (!Member.bIsActivePartyMember) { continue; }

        UPartyMemberEntryWidget* Entry =
            CreateWidget<UPartyMemberEntryWidget>(this, MemberEntryWidgetClass);
        if (!Entry) { continue; }

        Entry->SetMemberData(Member);
        Entry->SetLeader(Member.CharacterID == LeaderID);
        BP_AddActiveEntry(Entry);
        AllEntries.Add(Entry);
        ++ActiveEntryCount;
    }

    // Col 1: bench members
    for (const FPartyMemberData& Member : Data.Members)
    {
        if (Member.bIsActivePartyMember) { continue; }

        UPartyMemberEntryWidget* Entry =
            CreateWidget<UPartyMemberEntryWidget>(this, MemberEntryWidgetClass);
        if (!Entry) { continue; }

        Entry->SetMemberData(Member);
        Entry->SetLeader(false);
        BP_AddBenchEntry(Entry);
        AllEntries.Add(Entry);
    }

    CursorCol = 0;
    CursorRow = 0;
    ClampCursor();
    UpdateSelectionVisual();

    UE_LOG(LogTemp, Log,
        TEXT("[PartyManagerWidget] Refreshed: %d active, %d bench."),
        ActiveEntryCount, AllEntries.Num() - ActiveEntryCount);
}

// ---------------------------------------------------------------------------
// 2D Navigation
// ---------------------------------------------------------------------------

void UPartyManagerWidget::NavigateUp()
{
    if (GetColumnSize(CursorCol) == 0) { return; }

    CursorRow = FMath::Max(0, CursorRow - 1);
    UpdateSelectionVisual();

    UE_LOG(LogTemp, Verbose,
        TEXT("[PartyManagerWidget] NavigateUp -> col=%d row=%d"), CursorCol, CursorRow);
}

void UPartyManagerWidget::NavigateDown()
{
    const int32 ColSize = GetColumnSize(CursorCol);
    if (ColSize == 0) { return; }

    CursorRow = FMath::Min(ColSize - 1, CursorRow + 1);
    UpdateSelectionVisual();

    UE_LOG(LogTemp, Verbose,
        TEXT("[PartyManagerWidget] NavigateDown -> col=%d row=%d"), CursorCol, CursorRow);
}

void UPartyManagerWidget::NavigateLeft()
{
    if (CursorCol == 0) { return; }

    CursorCol = 0;
    ClampCursor();
    UpdateSelectionVisual();

    UE_LOG(LogTemp, Verbose,
        TEXT("[PartyManagerWidget] NavigateLeft -> col=%d row=%d"), CursorCol, CursorRow);
}

void UPartyManagerWidget::NavigateRight()
{
    if (CursorCol == 1) { return; }

    // Only move right if the bench column has at least one member
    if (GetColumnSize(1) == 0)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("[PartyManagerWidget] NavigateRight: bench is empty, ignoring."));
        return;
    }

    CursorCol = 1;
    ClampCursor();
    UpdateSelectionVisual();

    UE_LOG(LogTemp, Verbose,
        TEXT("[PartyManagerWidget] NavigateRight -> col=%d row=%d"), CursorCol, CursorRow);
}

// ---------------------------------------------------------------------------
// Confirm / Cancel
// ---------------------------------------------------------------------------

void UPartyManagerWidget::ConfirmSelection()
{
    const int32 Index = GetEntryIndex(CursorCol, CursorRow);
    if (Index == INDEX_NONE) { return; }

    UPartyMemberEntryWidget* Selected = AllEntries[Index];
    if (!Selected) { return; }

    const FGuid SelectedID = Selected->GetCharacterID();
    const bool  bIsActive  = (CursorCol == 0);

    // -----------------------------------------------------------------------
    // SWAP PENDING: step 2 — user is choosing which active member to replace
    // -----------------------------------------------------------------------
    if (PendingSwapIncomingID.IsValid())
    {
        if (bIsActive)
        {
            UE_LOG(LogTemp, Log,
                TEXT("[PartyManagerWidget] Swap confirmed: incoming=%s outgoing=%s"),
                *PendingSwapIncomingID.ToString(), *SelectedID.ToString());

            PartySubsystem->SwapActiveMembers(PendingSwapIncomingID, SelectedID);
            // RefreshPartyManager fires via OnRosterChanged; state is reset there.
        }
        else
        {
            // Confirmed on bench again — update the candidate
            PendingSwapIncomingID = SelectedID;
            BP_SetSwapPendingState(true, FText::GetEmpty());

            UE_LOG(LogTemp, Log,
                TEXT("[PartyManagerWidget] Swap candidate updated to ID=%s"),
                *SelectedID.ToString());
        }
        return;
    }

    // -----------------------------------------------------------------------
    // ACTIVE MEMBER SELECTED (step 1): user confirmed an active member
    // -----------------------------------------------------------------------
    if (ActiveConfirmState == EActiveConfirmState::Selected)
    {
        if (bIsActive)
        {
            if (SelectedID == PendingActiveID)
            {
                // Second confirm on the same active member → make them leader
                UE_LOG(LogTemp, Log,
                    TEXT("[PartyManagerWidget] SetPartyLeader: ID=%s"),
                    *SelectedID.ToString());

                PartySubsystem->SetPartyLeader(SelectedID);
                // OnRosterChanged fires RefreshPartyManager which resets state
            }
            else
            {
                // Different active member confirmed — update selection
                PendingActiveID = SelectedID;
                UpdateSelectionVisual();

                UE_LOG(LogTemp, Log,
                    TEXT("[PartyManagerWidget] Active selection updated to ID=%s"),
                    *SelectedID.ToString());
            }
        }
        else
        {
            // Confirmed on bench while an active member is selected →
            // treat bench member as incoming, selected active as outgoing
            UE_LOG(LogTemp, Log,
                TEXT("[PartyManagerWidget] Swap from active-select: outgoing=%s incoming=%s"),
                *PendingActiveID.ToString(), *SelectedID.ToString());

            PartySubsystem->SwapActiveMembers(SelectedID, PendingActiveID);
            // OnRosterChanged fires RefreshPartyManager which resets state
        }
        return;
    }

    // -----------------------------------------------------------------------
    // NO PENDING STATE: first input
    // -----------------------------------------------------------------------
    if (bIsActive)
    {
        // First confirm on an active member — enter selection state
        ActiveConfirmState = EActiveConfirmState::Selected;
        PendingActiveID    = SelectedID;
        UpdateSelectionVisual();

        UE_LOG(LogTemp, Log,
            TEXT("[PartyManagerWidget] Active member selected (step 1): ID=%s"),
            *SelectedID.ToString());
    }
    else
    {
        // Confirmed on bench with no state — auto-activate or begin swap
        const bool bHasOpenSlot = (PartySubsystem->GetActiveMemberCount() < 4);
        if (bHasOpenSlot)
        {
            UE_LOG(LogTemp, Log,
                TEXT("[PartyManagerWidget] Open slot — auto-activating bench ID=%s"),
                *SelectedID.ToString());

            PartySubsystem->SetMemberActive(SelectedID, true);
        }
        else
        {
            PendingSwapIncomingID = SelectedID;
            BP_SetSwapPendingState(true, FText::GetEmpty());

            UE_LOG(LogTemp, Log,
                TEXT("[PartyManagerWidget] Party full — swap initiated: bench ID=%s"),
                *SelectedID.ToString());
        }
    }
}

bool UPartyManagerWidget::CancelSelection()
{
    if (PendingSwapIncomingID.IsValid())
    {
        PendingSwapIncomingID.Invalidate();
        BP_SetSwapPendingState(false, FText::GetEmpty());
        UE_LOG(LogTemp, Log, TEXT("[PartyManagerWidget] Pending swap cancelled."));
        return true;
    }

    if (ActiveConfirmState == EActiveConfirmState::Selected)
    {
        ActiveConfirmState = EActiveConfirmState::None;
        PendingActiveID.Invalidate();
        UpdateSelectionVisual();
        UE_LOG(LogTemp, Log, TEXT("[PartyManagerWidget] Active selection cancelled."));
        return true;
    }

    UE_LOG(LogTemp, Log, TEXT("[PartyManagerWidget] CancelSelection: no pending state — close signal."));
    return false;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

int32 UPartyManagerWidget::GetEntryIndex(int32 InCol, int32 InRow) const
{
    // Col 0 occupies indices [0, ActiveEntryCount).
    // Col 1 occupies indices [ActiveEntryCount, AllEntries.Num()).
    if (InCol == 0)
    {
        if (InRow < 0 || InRow >= ActiveEntryCount) { return INDEX_NONE; }
        return InRow;
    }
    else
    {
        const int32 BenchCount = AllEntries.Num() - ActiveEntryCount;
        if (InRow < 0 || InRow >= BenchCount) { return INDEX_NONE; }
        return ActiveEntryCount + InRow;
    }
}

int32 UPartyManagerWidget::GetColumnSize(int32 InCol) const
{
    return (InCol == 0) ? ActiveEntryCount : (AllEntries.Num() - ActiveEntryCount);
}

void UPartyManagerWidget::ClampCursor()
{
    const int32 ColSize = GetColumnSize(CursorCol);
    if (ColSize == 0)
    {
        CursorRow = 0;
        return;
    }
    CursorRow = FMath::Clamp(CursorRow, 0, ColSize - 1);
}

void UPartyManagerWidget::UpdateSelectionVisual()
{
    for (int32 i = 0; i < AllEntries.Num(); ++i)
    {
        if (AllEntries[i])
        {
            // Determine which (col, row) this flat index maps to
            const bool bInActiveCol = (i < ActiveEntryCount);
            const int32 EntryCol    = bInActiveCol ? 0 : 1;
            const int32 EntryRow    = bInActiveCol ? i : (i - ActiveEntryCount);

            const bool bSelected = (EntryCol == CursorCol && EntryRow == CursorRow);
            AllEntries[i]->SetSelected(bSelected);
        }
    }
}