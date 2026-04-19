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
            TEXT("[PartyManagerWidget] MemberEntryWidgetClass not set — cannot build entries."));
        return;
    }

    BP_ClearEntries();
    AllEntries.Reset();
    ActiveEntryCount = 0;

    // Invalidate any pending swap on full rebuild to avoid stale GUIDs
    PendingSwapIncomingID.Invalidate();
    BP_SetSwapPendingState(false, FText::GetEmpty());

    const FPartyData& Data = PartySubsystem->GetPartyData();

    // --- Col 0: active members ---
    for (const FPartyMemberData& Member : Data.Members)
    {
        if (!Member.bIsActivePartyMember) { continue; }

        UPartyMemberEntryWidget* Entry = CreateWidget<UPartyMemberEntryWidget>(this, MemberEntryWidgetClass);
        if (!Entry) { continue; }

        Entry->SetMemberData(Member);
        BP_AddActiveEntry(Entry);
        AllEntries.Add(Entry);
        ++ActiveEntryCount;
    }

    // --- Col 1: bench members ---
    for (const FPartyMemberData& Member : Data.Members)
    {
        if (Member.bIsActivePartyMember) { continue; }

        UPartyMemberEntryWidget* Entry = CreateWidget<UPartyMemberEntryWidget>(this, MemberEntryWidgetClass);
        if (!Entry) { continue; }

        Entry->SetMemberData(Member);
        BP_AddBenchEntry(Entry);
        AllEntries.Add(Entry);
    }

    // Restore cursor to (0,0) after every rebuild
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

    // --- Step 2: swap pending — user is choosing who to replace ---
    if (PendingSwapIncomingID.IsValid())
    {
        if (bIsActive)
        {
            UE_LOG(LogTemp, Log,
                TEXT("[PartyManagerWidget] Swap confirmed: incoming=%s outgoing=%s"),
                *PendingSwapIncomingID.ToString(), *SelectedID.ToString());

            PartySubsystem->SwapActiveMembers(PendingSwapIncomingID, SelectedID);
            // RefreshPartyManager fires automatically via OnRosterChanged
        }
        else
        {
            // Confirmed on bench again — update the swap candidate
            PendingSwapIncomingID = SelectedID;
            BP_SetSwapPendingState(true, FText::GetEmpty());

            UE_LOG(LogTemp, Log,
                TEXT("[PartyManagerWidget] Swap candidate updated to ID=%s"), *SelectedID.ToString());
        }
        return;
    }

    // --- Step 1: no swap pending ---
    if (!bIsActive)
    {
        // If there is a free slot in the active party, fill it directly —
        // no need to ask the player who to replace.
        const bool bHasOpenSlot = (PartySubsystem->GetActiveMemberCount() < 4);
        if (bHasOpenSlot)
        {
            UE_LOG(LogTemp, Log,
                TEXT("[PartyManagerWidget] Open slot available — auto-activating bench member ID=%s"),
                *SelectedID.ToString());

            PartySubsystem->SetMemberActive(SelectedID, true);
            // RefreshPartyManager fires automatically via OnRosterChanged
        }
        else
        {
            // Party is full — begin two-step swap flow
            PendingSwapIncomingID = SelectedID;
            BP_SetSwapPendingState(true, FText::GetEmpty());

            UE_LOG(LogTemp, Log,
                TEXT("[PartyManagerWidget] Party full — swap initiated: bench member ID=%s"),
                *SelectedID.ToString());
        }
    }
    // Confirming an active member with no pending swap is a no-op for now
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

    UE_LOG(LogTemp, Log, TEXT("[PartyManagerWidget] CancelSelection: no pending swap — close signal."));
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