// Copyright Xyzto Works

#include "Widgets/ContainerTransferWidget.h"
#include "Actors/ContainerActor.h"
#include "Data/ContainerDefinition.h"
#include "Data/ItemDefinition.h"
#include "World/InventorySubsystem.h"
#include "World/ItemContainerSubsystem.h"

namespace
{
    // 6 columns is the agreed grid width for both panels — matches the player
    // inventory layout already in InventoryWidget.
    constexpr int32 GridColumns = 6;
}

// ===========================================================================
// Lifecycle
// ===========================================================================

void UContainerTransferWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UGameInstance* GI = GetGameInstance();
    InventorySubsystem = GI ? GI->GetSubsystem<UInventorySubsystem>() : nullptr;
    TransferSubsystem  = GI ? GI->GetSubsystem<UItemContainerSubsystem>() : nullptr;

    if (!InventorySubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[ContainerTransferWidget] NativeConstruct: InventorySubsystem missing."));
    }
    if (!TransferSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[ContainerTransferWidget] NativeConstruct: ItemContainerSubsystem missing."));
    }
}

void UContainerTransferWidget::NativeDestruct()
{
    if (bIsOpen)
    {
        Close();
    }
    Super::NativeDestruct();
}

// ===========================================================================
// Open / Close
// ===========================================================================

void UContainerTransferWidget::OpenForContainer(AContainerActor* InContainer)
{
    if (!InContainer)
    {
        UE_LOG(LogTemp, Error, TEXT("[ContainerTransferWidget] OpenForContainer: null container."));
        return;
    }
    if (!InventorySubsystem || !TransferSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[ContainerTransferWidget] OpenForContainer: subsystems missing."));
        return;
    }

    ActiveContainer = InContainer;
    bIsOpen = true;

    FocusedPanel     = EContainerPanel::PlayerInventory;
    FocusedSlotIndex = 0;
    bPromptOpen      = false;
    PendingPromptSlotIndex = INDEX_NONE;

    // Subscribe to both source change events so refreshes are automatic.
    // OnGoldChanged is bound separately because gold mutations don't fire
    // OnInventoryChanged — currency lives in PartyData, not RuntimeInventory.
    InventorySubsystem->OnInventoryChanged.AddUObject(this, &UContainerTransferWidget::HandlePlayerInventoryChanged);
    InventorySubsystem->OnGoldChanged.AddUObject(this, &UContainerTransferWidget::HandlePlayerGoldChanged);
    InContainer->OnContentsChanged.AddUObject(this, &UContainerTransferWidget::HandleContainerContentsChanged);

    SetVisibility(ESlateVisibility::Visible);
    BP_OnOpened();
    RefreshAll();
    BP_UpdateCursorVisual(FocusedPanel, FocusedSlotIndex);

    UE_LOG(LogTemp, Log, TEXT("[ContainerTransferWidget] Opened for container '%s'."),
        *InContainer->GetName());
}

void UContainerTransferWidget::Close()
{
    if (!bIsOpen) return;

    if (InventorySubsystem)
    {
        InventorySubsystem->OnInventoryChanged.RemoveAll(this);
        InventorySubsystem->OnGoldChanged.RemoveAll(this);
    }
    if (AContainerActor* Container = ActiveContainer.Get())
    {
        Container->OnContentsChanged.RemoveAll(this);
    }

    ActiveContainer = nullptr;
    bIsOpen         = false;
    bPromptOpen     = false;
    PendingPromptSlotIndex = INDEX_NONE;

    BP_HideQuantityPrompt();
    BP_OnClosed();
    SetVisibility(ESlateVisibility::Collapsed);

    UE_LOG(LogTemp, Log, TEXT("[ContainerTransferWidget] Closed."));
}

// ===========================================================================
// Navigation
// ===========================================================================

void UContainerTransferWidget::NavigateUp()
{
    if (bPromptOpen) return;
    if (FocusedSlotIndex - GridColumns >= 0)
    {
        FocusedSlotIndex -= GridColumns;
        BP_UpdateCursorVisual(FocusedPanel, FocusedSlotIndex);
    }
}

void UContainerTransferWidget::NavigateDown()
{
    if (bPromptOpen) return;

    const int32 PanelMaxSlots =
        (FocusedPanel == EContainerPanel::PlayerInventory)
            ? (InventorySubsystem ? InventorySubsystem->GetMaxSlots() : 0)
            : (ActiveContainer.IsValid() ? ActiveContainer->GetMaxSlots() : 0);

    if (FocusedSlotIndex + GridColumns < PanelMaxSlots)
    {
        FocusedSlotIndex += GridColumns;
        BP_UpdateCursorVisual(FocusedPanel, FocusedSlotIndex);
    }
}

void UContainerTransferWidget::NavigateLeft()
{
    if (bPromptOpen) return;

    // If we're on the leftmost column of the right panel, jump to the right
    // edge of the left panel at the same row. Otherwise just move one column.
    const int32 Col = FocusedSlotIndex % GridColumns;
    if (Col == 0 && FocusedPanel == EContainerPanel::Container)
    {
        FocusedPanel = EContainerPanel::PlayerInventory;
        const int32 Row = FocusedSlotIndex / GridColumns;
        FocusedSlotIndex = Row * GridColumns + (GridColumns - 1);
        ClampFocus();
        BP_UpdateCursorVisual(FocusedPanel, FocusedSlotIndex);
        return;
    }

    if (Col > 0)
    {
        FocusedSlotIndex -= 1;
        BP_UpdateCursorVisual(FocusedPanel, FocusedSlotIndex);
    }
}

void UContainerTransferWidget::NavigateRight()
{
    if (bPromptOpen) return;

    const int32 Col = FocusedSlotIndex % GridColumns;
    if (Col == GridColumns - 1 && FocusedPanel == EContainerPanel::PlayerInventory)
    {
        FocusedPanel = EContainerPanel::Container;
        const int32 Row = FocusedSlotIndex / GridColumns;
        FocusedSlotIndex = Row * GridColumns;
        ClampFocus();
        BP_UpdateCursorVisual(FocusedPanel, FocusedSlotIndex);
        return;
    }

    if (Col < GridColumns - 1)
    {
        FocusedSlotIndex += 1;
        ClampFocus();
        BP_UpdateCursorVisual(FocusedPanel, FocusedSlotIndex);
    }
}

void UContainerTransferWidget::FocusOnClicked(EContainerPanel ContainerFocusedPanel, int32 ContainerFocusedSlotIndex)
{
    FocusedPanel = ContainerFocusedPanel;
    FocusedSlotIndex = ContainerFocusedSlotIndex;
}

bool UContainerTransferWidget::ConfirmSelection()
{
    if (bPromptOpen) return false;

    const FInventoryEntry* Entry = GetFocusedEntry();
    if (!Entry || !Entry->IsValid())
    {
        UE_LOG(LogTemp, Verbose, TEXT("[ContainerTransferWidget] ConfirmSelection: focused slot is empty."));
        return false;
    }

    if (Entry->Quantity == 1)
    {
        DoTransfer(1);
        return true;
    }

    // Stack > 1 → defer to quantity prompt.
    PendingPromptSlotIndex = FocusedSlotIndex;
    PendingPromptPanel     = FocusedPanel;
    bPromptOpen            = true;

    UItemDefinition* Def = Entry->Definition.LoadSynchronous();
    BP_ShowQuantityPrompt(Def ? Def->DisplayName : FText::GetEmpty(), 1, Entry->Quantity);

    UE_LOG(LogTemp, Log, TEXT("[ContainerTransferWidget] Quantity prompt opened: '%s' (max=%d)."),
        Def ? *Def->DisplayName.ToString() : TEXT("null"),
        Entry->Quantity);
    return true;
}

bool UContainerTransferWidget::CancelSelection()
{
    if (bPromptOpen)
    {
        CancelQuantitySelection();
        return true;
    }
    // Nothing absorbing — controller should close us.
    return false;
}

// ===========================================================================
// Quantity prompt callbacks
// ===========================================================================

void UContainerTransferWidget::ConfirmQuantitySelection(int32 Quantity)
{
    if (!bPromptOpen)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ContainerTransferWidget] ConfirmQuantitySelection: no prompt open."));
        return;
    }

    // Restore focus to the slot the prompt was anchored on, in case the user
    // navigated while it was open (defensive — we block input during prompts).
    FocusedPanel     = PendingPromptPanel;
    FocusedSlotIndex = PendingPromptSlotIndex;

    const FInventoryEntry* Entry = GetFocusedEntry();
    if (!Entry || !Entry->IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ContainerTransferWidget] ConfirmQuantitySelection: pending entry vanished."));
        bPromptOpen = false;
        BP_HideQuantityPrompt();
        return;
    }

    const int32 ClampedQty = FMath::Clamp(Quantity, 1, Entry->Quantity);

    bPromptOpen = false;
    PendingPromptSlotIndex = INDEX_NONE;
    BP_HideQuantityPrompt();

    DoTransfer(ClampedQty);
}

void UContainerTransferWidget::CancelQuantitySelection()
{
    if (!bPromptOpen) return;

    bPromptOpen = false;
    PendingPromptSlotIndex = INDEX_NONE;
    BP_HideQuantityPrompt();

    UE_LOG(LogTemp, Log, TEXT("[ContainerTransferWidget] Quantity prompt cancelled."));
}

// ===========================================================================
// Subsystem handlers
// ===========================================================================

void UContainerTransferWidget::HandlePlayerInventoryChanged()
{
    if (!bIsOpen) return;
    RefreshAll();
}

void UContainerTransferWidget::HandlePlayerGoldChanged(int32 NewGold)
{
    if (!bIsOpen) return;
    // Re-emit the player header so the gold counter updates. We don't need a
    // full RefreshAll here — slots and weight are unaffected by gold changes.
    BP_SetPlayerHeader(
        InventorySubsystem->GetUsedSlots(),
        InventorySubsystem->GetMaxSlots(),
        InventorySubsystem->GetCurrentWeight(),
        InventorySubsystem->GetMaxWeight(),
        InventorySubsystem->IsOverweight(),
        NewGold);
}

void UContainerTransferWidget::HandleContainerContentsChanged()
{
    if (!bIsOpen) return;
    RefreshAll();
}

// ===========================================================================
// Internal helpers
// ===========================================================================

void UContainerTransferWidget::RefreshAll()
{
    if (!InventorySubsystem) return;

    BP_RefreshPlayerSlots(InventorySubsystem->GetItemEntries());
    BP_SetPlayerHeader(
        InventorySubsystem->GetUsedSlots(),
        InventorySubsystem->GetMaxSlots(),
        InventorySubsystem->GetCurrentWeight(),
        InventorySubsystem->GetMaxWeight(),
        InventorySubsystem->IsOverweight(),
        InventorySubsystem->GetGold());

    AContainerActor* Container = ActiveContainer.Get();
    if (Container)
    {
        BP_RefreshContainerSlots(Container->GetItemEntries());
        BP_SetContainerHeader(
            Container->GetSourceDisplayName(),
            Container->GetUsedSlots(),
            Container->GetMaxSlots(),
            Container->GetTotalWeight());
    }

    ClampFocus();
    BP_UpdateCursorVisual(FocusedPanel, FocusedSlotIndex);
}

const FInventoryEntry* UContainerTransferWidget::GetFocusedEntry() const
{
    return GetEntryAt(FocusedPanel, FocusedSlotIndex);
}

const FInventoryEntry* UContainerTransferWidget::GetEntryAt(EContainerPanel Panel, int32 SlotIndex) const
{
    if (Panel == EContainerPanel::PlayerInventory)
    {
        if (!InventorySubsystem) return nullptr;
        const TArray<FInventoryEntry>& Entries = InventorySubsystem->GetInventory();
        return Entries.IsValidIndex(SlotIndex) ? &Entries[SlotIndex] : nullptr;
    }
    AContainerActor* Container = ActiveContainer.Get();
    if (!Container) return nullptr;
    // Container's GetItemEntries returns a copy — index into the live Items
    // array would require a public accessor. For UI focus checks, indexing
    // a temporary copy each time is fine; the "live" mutation goes through
    // ProvideTransfer, which looks up by ItemDef anyway.
    static FInventoryEntry CachedEntry;
    const TArray<FInventoryEntry> Snapshot = Container->GetItemEntries();
    if (!Snapshot.IsValidIndex(SlotIndex)) return nullptr;
    CachedEntry = Snapshot[SlotIndex];
    return &CachedEntry;
}

int32 UContainerTransferWidget::GetUsedSlotsOnPanel(EContainerPanel Panel) const
{
    if (Panel == EContainerPanel::PlayerInventory)
    {
        return InventorySubsystem ? InventorySubsystem->GetUsedSlots() : 0;
    }
    return ActiveContainer.IsValid() ? ActiveContainer->GetUsedSlots() : 0;
}

void UContainerTransferWidget::ClampFocus()
{
    const int32 PanelMax =
        (FocusedPanel == EContainerPanel::PlayerInventory)
            ? (InventorySubsystem ? InventorySubsystem->GetMaxSlots() : 0)
            : (ActiveContainer.IsValid() ? ActiveContainer->GetMaxSlots() : 0);

    if (PanelMax <= 0) { FocusedSlotIndex = 0; return; }
    FocusedSlotIndex = FMath::Clamp(FocusedSlotIndex, 0, PanelMax - 1);
}

void UContainerTransferWidget::DoTransfer(int32 Quantity)
{
    if (!TransferSubsystem || !ActiveContainer.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[ContainerTransferWidget] DoTransfer: transfer subsystem or container missing."));
        return;
    }

    const FInventoryEntry* Entry = GetFocusedEntry();
    if (!Entry || !Entry->IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ContainerTransferWidget] DoTransfer: focused entry invalid."));
        return;
    }

    UItemDefinition* Def = Entry->Definition.LoadSynchronous();
    if (!Def)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ContainerTransferWidget] DoTransfer: entry has null definition."));
        return;
    }

    UObject* Source = nullptr;
    UObject* Dest   = nullptr;
    if (FocusedPanel == EContainerPanel::PlayerInventory)
    {
        Source = InventorySubsystem;
        Dest   = ActiveContainer.Get();
    }
    else
    {
        Source = ActiveContainer.Get();
        Dest   = InventorySubsystem;
    }

    const EInventoryResult Result = TransferSubsystem->TransferItem(Source, Dest, Def, Quantity);

    UE_LOG(LogTemp, Log, TEXT("[ContainerTransferWidget] DoTransfer: '%s' x%d, result=%d."),
        *Def->DisplayName.ToString(), Quantity, (int32)Result);

    BP_OnTransferResult(Result, FocusedPanel);
    // RefreshAll is driven by the subscribed delegates; no manual refresh needed.
}
