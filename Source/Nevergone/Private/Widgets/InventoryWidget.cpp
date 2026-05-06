// Copyright Xyzto Works

#include "Widgets/InventoryWidget.h"
#include "Widgets/InventoryContextMenuWidget.h"
#include "World/InventorySubsystem.h"
#include "World/ItemUsageSubsystem.h"
#include "World/CharacterPreviewSubsystem.h"
#include "GameInstance/MyGameInstance.h"
#include "GameInstance/MySaveGame.h"
#include "Data/PartyData.h"
#include "Data/ItemDefinition.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Engine/World.h"
#include "Characters/CharacterBase.h"

// ---------------------------------------------------------------------------
// NativeConstruct / NativeDestruct
// ---------------------------------------------------------------------------

void UInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UGameInstance* GI = GetGameInstance();
    InventorySubsystem = GI ? GI->GetSubsystem<UInventorySubsystem>() : nullptr;
    PreviewSubsystem   = GI ? GI->GetSubsystem<UCharacterPreviewSubsystem>() : nullptr;

    if (!InventorySubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryWidget] InventorySubsystem not found on construct."));
        return;
    }

    BindSubsystemDelegates();
    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Constructed and bound to InventorySubsystem."));
}

void UInventoryWidget::NativeDestruct()
{
    // Ensure the context menu is removed even if CloseInventory was never called
    // (e.g. the widget was destroyed directly while the menu was open).
    if (IsValid(ActiveContextMenuWidget))
    {
        ActiveContextMenuWidget->RemoveFromParent();
        ActiveContextMenuWidget = nullptr;
    }
    UnbindSubsystemDelegates();
    TeardownSceneCapture();
    Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Open / Close
// ---------------------------------------------------------------------------

void UInventoryWidget::OpenInventory()
{
    if (!InventorySubsystem) return;

    // Reset cursor to top-left of item grid on every open.
    ActivePanel    = EInventoryPanel::Left;
    CursorCol      = 0;
    CursorRow      = 0;
    EquipCursorRow = 0;
    bContextMenuOpen = false;

    SetVisibility(ESlateVisibility::Visible);
    ClampCharacterIndex();
    SetupSceneCapture();
    RefreshAll();
    BroadcastCursorVisual();

    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Opened. Character index %d."), SelectedCharacterIndex);
}

void UInventoryWidget::CloseInventory()
{
    TeardownSceneCapture();
    BP_HideContextMenu();
    bContextMenuOpen = false;

    // Explicitly remove and clear the C++ reference so the context menu
    // widget is always destroyed when the inventory closes, regardless of
    // what BP_HideContextMenu does with visibility.
    if (IsValid(ActiveContextMenuWidget))
    {
        ActiveContextMenuWidget->RemoveFromParent();
        ActiveContextMenuWidget = nullptr;
    }

    SetVisibility(ESlateVisibility::Collapsed);

    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Closed."));
}

// ---------------------------------------------------------------------------
// Input navigation
// ---------------------------------------------------------------------------

void UInventoryWidget::NavigateUp()
{
    if (bContextMenuOpen) return;

    if (ActivePanel == EInventoryPanel::Left)
    {
        CursorRow = FMath::Max(0, CursorRow - 1);
    }
    else
    {
        EquipCursorRow = FMath::Max(0, EquipCursorRow - 1);
    }

    BroadcastCursorVisual();
    UE_LOG(LogTemp, Verbose, TEXT("[InventoryWidget] NavigateUp — panel=%d col=%d row=%d equipRow=%d"),
        (int32)ActivePanel, CursorCol, CursorRow, EquipCursorRow);
}

void UInventoryWidget::NavigateDown()
{
    if (bContextMenuOpen) return;

    if (ActivePanel == EInventoryPanel::Left)
    {
        CursorRow = FMath::Min(GridRows - 1, CursorRow + 1);
    }
    else
    {
        EquipCursorRow = FMath::Min(EquipRows - 1, EquipCursorRow + 1);
    }

    BroadcastCursorVisual();
    UE_LOG(LogTemp, Verbose, TEXT("[InventoryWidget] NavigateDown — panel=%d col=%d row=%d equipRow=%d"),
        (int32)ActivePanel, CursorCol, CursorRow, EquipCursorRow);
}

void UInventoryWidget::NavigateLeft()
{
    if (bContextMenuOpen) return;

    if (ActivePanel == EInventoryPanel::Right)
    {
        // Return to left panel, preserving the row clamped to valid grid rows.
        ActivePanel = EInventoryPanel::Left;
        CursorCol   = GridCols - 1;
        CursorRow   = FMath::Clamp(EquipCursorRow, 0, GridRows - 1);
    }
    else
    {
        CursorCol = FMath::Max(0, CursorCol - 1);
    }

    BroadcastCursorVisual();
}

void UInventoryWidget::NavigateRight()
{
    if (bContextMenuOpen) return;

    if (ActivePanel == EInventoryPanel::Left)
    {
        if (CursorCol < GridCols - 1)
        {
            CursorCol++;
        }
        else
        {
            // Edge of grid — jump to right equipment panel.
            ActivePanel    = EInventoryPanel::Right;
            EquipCursorRow = FMath::Clamp(CursorRow, 0, EquipRows - 1);
        }
    }
    // Already on right panel — no further right navigation.

    BroadcastCursorVisual();
}

bool UInventoryWidget::ConfirmSelection()
{
    if (bContextMenuOpen) return false;

    OpenContextMenuForFocusedSlot();
    return true;
}

bool UInventoryWidget::CancelSelection()
{
    if (bContextMenuOpen)
    {
        BP_HideContextMenu();
        bContextMenuOpen = false;
        UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Context menu closed via Cancel."));
        return true;
    }

    // Nothing open — signal controller to close the inventory.
    return false;
}

// ---------------------------------------------------------------------------
// Character selection
// ---------------------------------------------------------------------------

void UInventoryWidget::SelectNextCharacter()
{
    TArray<FPartyMemberData*> Members = GetActivePartyMembers();
    if (Members.IsEmpty()) return;

    SelectedCharacterIndex = (SelectedCharacterIndex + 1) % Members.Num();
    SetupSceneCapture();

    if (const FPartyMemberData* Member = Members[SelectedCharacterIndex])
    {
        BP_SetCharacterInfo(Member->DisplayName, Member->ClassName);
        BP_RefreshEquipmentPanel(SelectedCharacterIndex, Member->EquipmentLoadout);
        BP_SetCharacterStats(FMath::RoundToInt(Member->CurrentHP), FMath::RoundToInt(Member->MaxHP), Member->bIsAlive);
        BP_RefreshCharacterNavigation(SelectedCharacterIndex, Members.Num());
    }
    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Next character → index %d."), SelectedCharacterIndex);
}

void UInventoryWidget::SelectPreviousCharacter()
{
    TArray<FPartyMemberData*> Members = GetActivePartyMembers();
    if (Members.IsEmpty()) return;

    SelectedCharacterIndex = (SelectedCharacterIndex - 1 + Members.Num()) % Members.Num();
    SetupSceneCapture();

    if (const FPartyMemberData* Member = Members[SelectedCharacterIndex])
    {
        BP_SetCharacterInfo(Member->DisplayName, Member->ClassName);
        BP_RefreshEquipmentPanel(SelectedCharacterIndex, Member->EquipmentLoadout);
        BP_SetCharacterStats(FMath::RoundToInt(Member->CurrentHP), FMath::RoundToInt(Member->MaxHP), Member->bIsAlive);
        BP_RefreshCharacterNavigation(SelectedCharacterIndex, Members.Num());
    }
    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Previous character → index %d."), SelectedCharacterIndex);
}

void UInventoryWidget::SelectCharacterByIndex(int32 Index)
{
    TArray<FPartyMemberData*> Members = GetActivePartyMembers();
    if (!Members.IsValidIndex(Index)) return;

    SelectedCharacterIndex = Index;
    SetupSceneCapture();

    if (const FPartyMemberData* Member = Members[Index])
    {
        BP_SetCharacterInfo(Member->DisplayName, Member->ClassName);
        BP_RefreshEquipmentPanel(SelectedCharacterIndex, Member->EquipmentLoadout);
        BP_SetCharacterStats(FMath::RoundToInt(Member->CurrentHP), FMath::RoundToInt(Member->MaxHP), Member->bIsAlive);
        BP_RefreshCharacterNavigation(SelectedCharacterIndex, Members.Num());
    }
}

// ---------------------------------------------------------------------------
// RegisterContextMenuWidget
// ---------------------------------------------------------------------------

void UInventoryWidget::RegisterContextMenuWidget(UInventoryContextMenuWidget* Widget)
{
    if (!Widget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryWidget] RegisterContextMenuWidget: null widget."));
        return;
    }
    Widget->SetOwnerInventoryWidget(this);
    ActiveContextMenuWidget = Widget;
    bContextMenuOpen = true;
    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Context menu widget registered."));
}

// ---------------------------------------------------------------------------
// Slot interaction — mouse path (OriginWidget provides geometry for anchoring)
// ---------------------------------------------------------------------------

void UInventoryWidget::OnInventorySlotClicked(int32 SlotIndex, UWidget* OriginWidget)
{
    if (!InventorySubsystem) return;

    const TArray<FInventoryEntry>& Entries = InventorySubsystem->GetInventory();
    if (!Entries.IsValidIndex(SlotIndex) || !Entries[SlotIndex].IsValid()) return;

    // Sync cursor to the clicked slot so joystick/mouse stay consistent.
    ActivePanel = EInventoryPanel::Left;
    CursorCol   = SlotIndex % GridCols;
    CursorRow   = SlotIndex / GridCols;
    BroadcastCursorVisual();

    PendingSlotIndex      = SlotIndex;
    PendingEquipmentSlot  = EEquipmentSlot::None;
    bPendingIsConsumable  = false;
    PendingConsumableSlot = INDEX_NONE;

    const FInventoryEntry& Entry = Entries[SlotIndex];
    const UItemDefinition* Def   = Entry.Definition.Get();
    const bool bCanEquip = Def && Def->Category == EItemCategory::Equipment;
    const bool bCanUse   = Def && Def->Category == EItemCategory::Consumable;
    const bool bCanDrop  = Def && !Def->bIsKeyItem;

    CapturePositionFromWidget(OriginWidget);
    BP_ShowContextMenu(Entry, bCanEquip, bCanUse, bCanDrop);

    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Slot %d clicked — '%s' — menu pos (%.0f, %.0f)."),
        SlotIndex, Def ? *Def->DisplayName.ToString() : TEXT("empty"),
        LastContextMenuPosition.X, LastContextMenuPosition.Y);
}

void UInventoryWidget::OnEquipmentSlotClicked(EEquipmentSlot EquipSlot, UWidget* OriginWidget)
{
    // Sync cursor to the clicked equipment slot.
    ActivePanel    = EInventoryPanel::Right;
    EquipCursorRow = (int32)EquipSlot - 1; // EEquipmentSlot: None=0, Armor=1, Weapon=2, Necklace=3, Belt=4
    EquipCursorRow = FMath::Clamp(EquipCursorRow, 0, EquipRows - 1);
    BroadcastCursorVisual();

    PendingSlotIndex      = INDEX_NONE;
    PendingEquipmentSlot  = EquipSlot;
    bPendingIsConsumable  = false;
    PendingConsumableSlot = INDEX_NONE;

    FInventoryEntry DummyEntry;
    CapturePositionFromWidget(OriginWidget);
    BP_ShowContextMenu(DummyEntry, false, false, false);

    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Equipment slot %d clicked — menu pos (%.0f, %.0f)."),
        (int32)EquipSlot, LastContextMenuPosition.X, LastContextMenuPosition.Y);
}

void UInventoryWidget::OnConsumableSlotClicked(int32 SlotIndex, UWidget* OriginWidget)
{
    // Consumable slots occupy right panel rows 4-7.
    ActivePanel    = EInventoryPanel::Right;
    EquipCursorRow = 4 + SlotIndex;
    BroadcastCursorVisual();

    PendingSlotIndex      = INDEX_NONE;
    PendingEquipmentSlot  = EEquipmentSlot::None;
    bPendingIsConsumable  = true;
    PendingConsumableSlot = SlotIndex;

    FInventoryEntry DummyEntry;
    CapturePositionFromWidget(OriginWidget);
    BP_ShowContextMenu(DummyEntry, false, false, false);

    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Consumable slot %d clicked — menu pos (%.0f, %.0f)."),
        SlotIndex, LastContextMenuPosition.X, LastContextMenuPosition.Y);
}

// ---------------------------------------------------------------------------
// Context menu callbacks
// ---------------------------------------------------------------------------

void UInventoryWidget::OnContextMenuEquip()
{
    bContextMenuOpen = false;
    if (!InventorySubsystem || !Entries_IsValidPendingSlot()) { BP_HideContextMenu(); return; }

    const UItemDefinition* Def = InventorySubsystem->GetInventory()[PendingSlotIndex].Definition.Get();
    if (!Def) { BP_HideContextMenu(); return; }

    const FGuid CharGUID = GetSelectedCharacterGUID();
    const EInventoryResult Result = InventorySubsystem->EquipItem(CharGUID, const_cast<UItemDefinition*>(Def));

    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Equip '%s' result=%d."), *Def->DisplayName.ToString(), (int32)Result);

    BP_HideContextMenu();
    RefreshAll();
}

void UInventoryWidget::OnContextMenuEquipConsumable(int32 TargetSlotIndex)
{
    bContextMenuOpen = false;
    if (!InventorySubsystem || !Entries_IsValidPendingSlot()) { BP_HideContextMenu(); return; }

    const UItemDefinition* Def = InventorySubsystem->GetInventory()[PendingSlotIndex].Definition.Get();
    if (!Def) { BP_HideContextMenu(); return; }

    const FGuid CharGUID = GetSelectedCharacterGUID();
    InventorySubsystem->EquipConsumable(CharGUID, TargetSlotIndex, const_cast<UItemDefinition*>(Def));

    BP_HideContextMenu();
    RefreshAll();
}

void UInventoryWidget::OnContextMenuUse()
{
    bContextMenuOpen = false;
    if (!InventorySubsystem || !Entries_IsValidPendingSlot()) { BP_HideContextMenu(); return; }

    UItemDefinition* Def = const_cast<UItemDefinition*>(
        InventorySubsystem->GetInventory()[PendingSlotIndex].Definition.Get());
    if (!Def) { BP_HideContextMenu(); return; }

    // Resolve the agnostic usage subsystem — the same one the future combat
    // Use Item skill will call. The inventory only fills in the request and
    // shows feedback; all consumption / effect logic lives in the subsystem.
    UGameInstance* GI = GetGameInstance();
    UItemUsageSubsystem* UsageSubsystem = GI ? GI->GetSubsystem<UItemUsageSubsystem>() : nullptr;

    EItemUseResult Result = EItemUseResult::FailedNullItem;
    if (!UsageSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventoryWidget] OnContextMenuUse: ItemUsageSubsystem missing."));
    }
    else
    {
        FItemUseRequest Request;
        Request.ItemDef           = Def;
        Request.TargetCharacterID = GetSelectedCharacterGUID();
        Request.Caster            = nullptr;
        Request.Source            = EItemUseSource::Inventory;
        Request.EquippedSlotIndex = INDEX_NONE;

        Result = UsageSubsystem->TryUseItem(Request);
    }

    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Use '%s' result=%d."),
        *Def->DisplayName.ToString(), (int32)Result);

    BP_OnItemUseResult(Def, Result);
    BP_HideContextMenu();
    RefreshAll();
}

void UInventoryWidget::OnContextMenuDrop()
{
    bContextMenuOpen = false;
    if (!InventorySubsystem || !Entries_IsValidPendingSlot()) { BP_HideContextMenu(); return; }

    const UItemDefinition* Def = InventorySubsystem->GetInventory()[PendingSlotIndex].Definition.Get();
    if (!Def) { BP_HideContextMenu(); return; }

    APlayerController* PC = GetOwningPlayer();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Pawn) { BP_HideContextMenu(); return; }

    // Project the spawn point onto the ground with a downward line trace.
    // This prevents items from floating at the pawn's capsule center height.
    const FVector ForwardDrop = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 100.f;
    FVector SpawnLoc = ForwardDrop;

    if (UWorld* World = GetWorld())
    {
        FHitResult Hit;
        const FVector TraceStart = ForwardDrop + FVector(0.f, 0.f, 200.f);
        const FVector TraceEnd   = ForwardDrop - FVector(0.f, 0.f, 500.f);

        if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic))
        {
            // Small Z offset so the item sits just above the surface.
            SpawnLoc = Hit.ImpactPoint + FVector(0.f, 0.f, 10.f);
        }
    }

    InventorySubsystem->DropItem(const_cast<UItemDefinition*>(Def), SpawnLoc);

    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Dropped '%s'."), *Def->DisplayName.ToString());

    BP_HideContextMenu();
    RefreshAll();
}

void UInventoryWidget::OnContextMenuCancel()
{
    bContextMenuOpen = false;
    PendingSlotIndex      = INDEX_NONE;
    PendingEquipmentSlot  = EEquipmentSlot::None;
    bPendingIsConsumable  = false;
    PendingConsumableSlot = INDEX_NONE;
    BP_HideContextMenu();
}

// ---------------------------------------------------------------------------
// Sort
// ---------------------------------------------------------------------------

void UInventoryWidget::SetSortLayers(const TArray<FInventorySortLayer>& NewLayers)
{
    if (InventorySubsystem) InventorySubsystem->SetSortLayers(NewLayers);
}

// ---------------------------------------------------------------------------
// Weight helpers
// ---------------------------------------------------------------------------

float UInventoryWidget::GetCurrentWeight() const
{
    return InventorySubsystem ? InventorySubsystem->GetCurrentWeight() : 0.f;
}

float UInventoryWidget::GetMaxWeight() const
{
    return InventorySubsystem ? InventorySubsystem->GetMaxWeight() : 0.f;
}

// ---------------------------------------------------------------------------
// Delegate binding
// ---------------------------------------------------------------------------

void UInventoryWidget::BindSubsystemDelegates()
{
    if (!InventorySubsystem) return;
    InventorySubsystem->OnInventoryChanged.AddUObject(this, &UInventoryWidget::HandleInventoryChanged);
    InventorySubsystem->OnWeightChanged.AddUObject(this, &UInventoryWidget::HandleWeightChanged);
    InventorySubsystem->OnGoldChanged.AddUObject(this, &UInventoryWidget::HandleGoldChanged);
}

void UInventoryWidget::UnbindSubsystemDelegates()
{
    if (!InventorySubsystem) return;
    InventorySubsystem->OnInventoryChanged.RemoveAll(this);
    InventorySubsystem->OnWeightChanged.RemoveAll(this);
    InventorySubsystem->OnGoldChanged.RemoveAll(this);
}

void UInventoryWidget::HandleInventoryChanged() { RefreshAll(); }

void UInventoryWidget::HandleWeightChanged(float NewWeight)
{
    BP_RefreshWeightDisplay(NewWeight, GetMaxWeight(), InventorySubsystem->IsOverweight());
}

void UInventoryWidget::HandleGoldChanged(int32 NewGold)
{
    BP_RefreshGoldDisplay(NewGold);
}

// ---------------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------------

void UInventoryWidget::RefreshAll()
{
    if (!InventorySubsystem) return;

    BP_RefreshInventorySlots(InventorySubsystem->GetInventory());
    BP_RefreshWeightDisplay(GetCurrentWeight(), GetMaxWeight(), InventorySubsystem->IsOverweight());
    BP_RefreshGoldDisplay(InventorySubsystem->GetGold());

    TArray<FPartyMemberData*> Members = GetActivePartyMembers();
    ClampCharacterIndex();

    if (Members.IsValidIndex(SelectedCharacterIndex))
    {
        if (const FPartyMemberData* Member = Members[SelectedCharacterIndex])
        {
            BP_SetCharacterInfo(Member->DisplayName, Member->ClassName);
            BP_RefreshEquipmentPanel(SelectedCharacterIndex, Member->EquipmentLoadout);
            BP_SetCharacterStats(FMath::RoundToInt(Member->CurrentHP), FMath::RoundToInt(Member->MaxHP), Member->bIsAlive);
            BP_RefreshCharacterNavigation(SelectedCharacterIndex, Members.Num());
        }
    }
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// SceneCapture — delegates entirely to UCharacterPreviewSubsystem
// ---------------------------------------------------------------------------

void UInventoryWidget::SetupSceneCapture()
{
    if (!PreviewSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryWidget] SetupSceneCapture: PreviewSubsystem is null."));
        return;
    }

    TArray<FPartyMemberData*> Members = GetActivePartyMembers();
    if (!Members.IsValidIndex(SelectedCharacterIndex)) { return; }

    const FPartyMemberData* Member = Members[SelectedCharacterIndex];
    if (!Member || !Member->CharacterClass) { return; }

    PreviewSubsystem->ShowCharacterClass(Member->CharacterClass);

    UTextureRenderTarget2D* RT = PreviewSubsystem->GetPreviewTexture();
    if (RT)
    {
        BP_SetCharacterRenderTarget(RT);
    }

    UE_LOG(LogTemp, Log,
        TEXT("[InventoryWidget] Preview updated for character index %d."),
        SelectedCharacterIndex);
}

void UInventoryWidget::TeardownSceneCapture()
{
    // The studio persists for the lifetime of the game instance — nothing
    // to destroy here. The subsystem manages its own lifecycle.
    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] SceneCapture torn down."));
}

// ---------------------------------------------------------------------------
// Context menu positioning
// ---------------------------------------------------------------------------

void UInventoryWidget::CapturePositionFromWidget(UWidget* OriginWidget)
{
    if (!OriginWidget) return;

    const FGeometry& Geo         = OriginWidget->GetCachedGeometry();
    const FVector2D  AbsTopLeft  = Geo.GetAbsolutePosition();
    const FVector2D  AbsSize     = Geo.GetAbsoluteSize();

    // Anchor the menu at the bottom-left of the slot, offset down by a small gap.
    constexpr float VerticalGap = 1.f;
    LastContextMenuPosition = FVector2D(AbsTopLeft.X, AbsTopLeft.Y + AbsSize.Y + VerticalGap);
}

// ---------------------------------------------------------------------------
// Navigation helpers
// ---------------------------------------------------------------------------

void UInventoryWidget::BroadcastCursorVisual()
{
    const int32 FlatIndex = (ActivePanel == EInventoryPanel::Left)
        ? GetFocusedInventorySlotIndex()
        : 30 + EquipCursorRow; // Right panel slots are 30-37

    BP_UpdateCursorVisual(ActivePanel,
        ActivePanel == EInventoryPanel::Left ? CursorCol : 0,
        ActivePanel == EInventoryPanel::Left ? CursorRow : EquipCursorRow,
        FlatIndex);
}

int32 UInventoryWidget::GetFocusedInventorySlotIndex() const
{
    if (!InventorySubsystem) return INDEX_NONE;

    const int32 Index = CursorRow * GridCols + CursorCol;
    const TArray<FInventoryEntry>& Entries = InventorySubsystem->GetInventory();

    if (!Entries.IsValidIndex(Index)) return INDEX_NONE;
    return Index;
}

EEquipmentSlot UInventoryWidget::GetFocusedEquipmentSlot() const
{
    // Rows 0-3 map to the four gear slots in declaration order.
    switch (EquipCursorRow)
    {
    case 0: return EEquipmentSlot::Armor;
    case 1: return EEquipmentSlot::Weapon;
    case 2: return EEquipmentSlot::Necklace;
    case 3: return EEquipmentSlot::Belt;
    default: return EEquipmentSlot::None; // Rows 4-7 are consumable slots.
    }
}

void UInventoryWidget::OpenContextMenuForFocusedSlot()
{
    if (ActivePanel == EInventoryPanel::Left)
    {
        const int32 SlotIndex = GetFocusedInventorySlotIndex();
        if (SlotIndex == INDEX_NONE) return;

        if (!InventorySubsystem) return;
        const TArray<FInventoryEntry>& Entries = InventorySubsystem->GetInventory();
        if (!Entries.IsValidIndex(SlotIndex) || !Entries[SlotIndex].IsValid()) return;

        PendingSlotIndex      = SlotIndex;
        PendingEquipmentSlot  = EEquipmentSlot::None;
        bPendingIsConsumable  = false;
        PendingConsumableSlot = INDEX_NONE;

        const FInventoryEntry& Entry = Entries[SlotIndex];
        const UItemDefinition* Def   = Entry.Definition.Get();
        const bool bCanEquip = Def && Def->Category == EItemCategory::Equipment;
        const bool bCanUse   = Def && Def->Category == EItemCategory::Consumable;
        const bool bCanDrop  = Def && !Def->bIsKeyItem;

        // For input navigation, position is derived from the slot's index geometry.
        // BP_UpdateCursorVisual keeps the focused slot highlighted, so BP can
        // pass the slot widget reference via GetContextMenuPosition if needed.
        // As a fallback, set to zero — BP should always use GetContextMenuPosition.
        LastContextMenuPosition = FVector2D::ZeroVector;
        BP_ShowContextMenu(Entry, bCanEquip, bCanUse, bCanDrop);

        UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] ConfirmSelection: slot %d — '%s'."),
            SlotIndex, Def ? *Def->DisplayName.ToString() : TEXT("empty"));
    }
    else // Right panel
    {
        const EEquipmentSlot FocusedSlot = GetFocusedEquipmentSlot();

        if (FocusedSlot != EEquipmentSlot::None)
        {
            // Gear slot
            PendingSlotIndex      = INDEX_NONE;
            PendingEquipmentSlot  = FocusedSlot;
            bPendingIsConsumable  = false;
            PendingConsumableSlot = INDEX_NONE;
        }
        else
        {
            // Consumable slot (rows 4-7)
            const int32 ConsumableIndex = EquipCursorRow - 4;
            PendingSlotIndex      = INDEX_NONE;
            PendingEquipmentSlot  = EEquipmentSlot::None;
            bPendingIsConsumable  = true;
            PendingConsumableSlot = ConsumableIndex;
        }

        LastContextMenuPosition = FVector2D::ZeroVector;
        FInventoryEntry DummyEntry;
        BP_ShowContextMenu(DummyEntry, false, false, false);

        UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] ConfirmSelection: equipment row %d."), EquipCursorRow);
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

FGuid UInventoryWidget::GetSelectedCharacterGUID() const
{
    TArray<FPartyMemberData*> Members = const_cast<UInventoryWidget*>(this)->GetActivePartyMembers();
    if (Members.IsValidIndex(SelectedCharacterIndex))
    {
        return Members[SelectedCharacterIndex]->CharacterID;
    }
    return FGuid();
}

TArray<FPartyMemberData*> UInventoryWidget::GetActivePartyMembers() const
{
    TArray<FPartyMemberData*> Result;
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) return Result;

    FPartyData& Party = const_cast<FPartyData&>(GI->GetPartyData());
    for (FPartyMemberData& Member : Party.Members)
    {
        if (Member.bIsActivePartyMember) Result.Add(&Member);
    }
    return Result;
}

void UInventoryWidget::ClampCharacterIndex()
{
    const int32 Count = GetActivePartyMembers().Num();
    if (Count == 0) { SelectedCharacterIndex = 0; return; }
    SelectedCharacterIndex = FMath::Clamp(SelectedCharacterIndex, 0, Count - 1);
}

bool UInventoryWidget::Entries_IsValidPendingSlot() const
{
    if (!InventorySubsystem) return false;
    return InventorySubsystem->GetInventory().IsValidIndex(PendingSlotIndex);
}
