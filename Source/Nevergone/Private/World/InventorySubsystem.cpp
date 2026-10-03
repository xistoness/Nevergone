// Copyright Xyzto Works

#include "World/InventorySubsystem.h"
#include "Data/ItemDefinition.h"
#include "Data/StatusEffectDefinition.h"
#include "Data/PartyData.h"
#include "GameInstance/MyGameInstance.h"
#include "GameInstance/MySaveGame.h"
#include "Characters/CharacterBase.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Level/WorldItemActor.h"
#include "Algo/Sort.h"
#include "EngineUtils.h"
#include "Data/UnitDefinition.h"
#include "Party/PartyManagerSubsystem.h"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UInventorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Pull the Overweight status asset reference from MyGameInstance,
    // since EditDefaultsOnly is not available on UGameInstanceSubsystem.
    UMyGameInstance* GI = GetGameInstance();
    if (GI)
    {
        OverweightStatusDefinition = GI->OverweightStatusDefinition;
    }

    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] Initialized."));
}

void UInventorySubsystem::Deinitialize()
{
    Super::Deinitialize();
}

void UInventorySubsystem::LoadFromSave()
{
    UMyGameInstance* GI = GetGameInstance();
    if (!GI) { UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] LoadFromSave: GameInstance not found.")); return; }

    UMySaveGame* Save = GI->GetActiveSave();
    if (!Save) { UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] LoadFromSave: No active save.")); return; }

    RuntimeInventory = Save->SharedInventory;
    SortLayers       = Save->InventorySortLayers;

    ApplySort();
    RecalculateWeightAndUpdateStatus();

    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] Loaded %d inventory entries from save."), RuntimeInventory.Num());
}

void UInventorySubsystem::FlushToSave()
{
    UMyGameInstance* GI = GetGameInstance();
    if (!GI) { UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] FlushToSave: GameInstance not found.")); return; }

    UMySaveGame* Save = GI->GetActiveSave();
    if (!Save) { UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] FlushToSave: No active save.")); return; }

    Save->SharedInventory      = RuntimeInventory;
    Save->InventorySortLayers  = SortLayers;

    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] Flushed %d inventory entries to save."), RuntimeInventory.Num());
}

// ---------------------------------------------------------------------------
// AddItem
// ---------------------------------------------------------------------------

EInventoryResult UInventorySubsystem::AddItem(UItemDefinition* ItemDef, int32 Quantity)
{
    if (!ItemDef || Quantity <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] AddItem: Invalid parameters."));
        return EInventoryResult::FailedNotFound;
    }

    // Currency interception: gold (and any other Currency-category items) never
    // occupy a slot. Convert to gold counter and bypass the rest of the pipeline.
    // World pickups (UWorldItemActor) flow through here, so chest-style "coin
    // pile" actors in the level "just work" without special-casing them.
    if (ItemDef->Category == EItemCategory::Currency)
    {
        AddGold(Quantity);
        return EInventoryResult::Success;
    }

    // Weight check: would adding Quantity units exceed MaxWeight?
    const float AddedWeight = ItemDef->WeightPerUnit * Quantity;
    if (GetCurrentWeight() + AddedWeight > GetMaxWeight())
    {
        UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] AddItem: Overweight — cannot add %s x%d."),
            *ItemDef->DisplayName.ToString(), Quantity);
        return EInventoryResult::FailedOverweight;
    }

    // Try to merge into existing stacks first, from the first matching slot.
    int32 Remaining = Quantity;
    for (FInventoryEntry& Entry : RuntimeInventory)
    {
        if (Remaining <= 0) break;

        UItemDefinition* Loaded = Entry.Definition.LoadSynchronous();
        if (Loaded != ItemDef) continue;

        const int32 Space = ItemDef->MaxStackSize - Entry.Quantity;
        if (Space <= 0) continue;

        const int32 Merge = FMath::Min(Space, Remaining);
        Entry.Quantity += Merge;
        Remaining      -= Merge;
    }

    // Open new slots for anything that didn't fit into existing stacks.
    while (Remaining > 0)
    {
        if (RuntimeInventory.Num() >= MaxInventorySlots)
        {
            UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] AddItem: Inventory full — %d units of %s not added."),
                Remaining, *ItemDef->DisplayName.ToString());
            return EInventoryResult::FailedInventoryFull;
        }

        FInventoryEntry NewEntry;
        NewEntry.Definition        = ItemDef;
        NewEntry.Quantity          = FMath::Min(Remaining, ItemDef->MaxStackSize);
        NewEntry.AcquiredTimestamp = FDateTime::UtcNow();
        RuntimeInventory.Add(NewEntry);
        Remaining -= NewEntry.Quantity;
    }

    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] AddItem: Added %s x%d. Total slots: %d."),
        *ItemDef->DisplayName.ToString(), Quantity, RuntimeInventory.Num());

    ApplySort();
    RecalculateWeightAndUpdateStatus();
    OnInventoryChanged.Broadcast();
    return EInventoryResult::Success;
}

// ---------------------------------------------------------------------------
// RemoveItem
// ---------------------------------------------------------------------------

EInventoryResult UInventorySubsystem::RemoveItem(UItemDefinition* ItemDef, int32 Quantity)
{
    if (!ItemDef || Quantity <= 0) return EInventoryResult::FailedNotFound;

    int32 Remaining = Quantity;
    for (int32 i = RuntimeInventory.Num() - 1; i >= 0 && Remaining > 0; --i)
    {
        UItemDefinition* Loaded = RuntimeInventory[i].Definition.LoadSynchronous();
        if (Loaded != ItemDef) continue;

        const int32 Take = FMath::Min(RuntimeInventory[i].Quantity, Remaining);
        RuntimeInventory[i].Quantity -= Take;
        Remaining -= Take;

        if (RuntimeInventory[i].Quantity <= 0)
        {
            RuntimeInventory.RemoveAt(i);
        }
    }

    if (Remaining == Quantity)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] RemoveItem: %s not found in inventory."),
            *ItemDef->DisplayName.ToString());
        return EInventoryResult::FailedNotFound;
    }

    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] RemoveItem: Removed %s x%d."),
        *ItemDef->DisplayName.ToString(), Quantity - Remaining);

    RecalculateWeightAndUpdateStatus();
    OnInventoryChanged.Broadcast();
    return EInventoryResult::Success;
}

// ---------------------------------------------------------------------------
// EquipItem
// ---------------------------------------------------------------------------

EInventoryResult UInventorySubsystem::EquipItem(const FGuid& CharacterID, UItemDefinition* ItemDef)
{
    if (!ItemDef) return EInventoryResult::FailedNotFound;

    if (ItemDef->Category != EItemCategory::Equipment)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] EquipItem: %s is not an equipment item."),
            *ItemDef->DisplayName.ToString());
        return EInventoryResult::FailedWrongCategory;
    }

    if (ItemDef->EquipmentSlot == EEquipmentSlot::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] EquipItem: %s has no equipment slot set."),
            *ItemDef->DisplayName.ToString());
        return EInventoryResult::FailedWrongCategory;
    }

    // Class restriction check.
    FPartyMemberData* Member = FindPartyMember(CharacterID);
    if (!Member)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] EquipItem: Party member %s not found."),
            *CharacterID.ToString());
        return EInventoryResult::FailedNotFound;
    }

    // Resolve the character's class for equipment restriction checks.
    // Primary source: live actor's UnitDefinition (most up-to-date).
    // Fallback: FPartyMemberData::UnitClass populated by ResolveDisplayName —
    // reliable when the actor is not spawned (e.g. during exploration inventory use).
    EUnitClass MemberClass = Member->UnitClass;
    ACharacterBase* CharActor = FindCharacterActor(CharacterID);
    if (CharActor && CharActor->GetUnitStats() && CharActor->GetUnitStats()->GetDefinition())
    {
        MemberClass = CharActor->GetUnitStats()->GetDefinition()->UnitClass;
    }

    UE_LOG(LogTemp, Log,
        TEXT("[InventorySubsystem] EquipItem: CharID=%s MemberClass=%d (actor %s) AllowedClasses=%d item=%s."),
        *CharacterID.ToString(),
        (int32)MemberClass,
        CharActor ? TEXT("found") : TEXT("not found — using cached class"),
        ItemDef->AllowedClasses.Num(),
        *ItemDef->DisplayName.ToString());

    if (!ItemDef->CanBeEquippedByClass(MemberClass))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[InventorySubsystem] EquipItem: Class restriction — MemberClass=%d not in AllowedClasses for '%s'."),
            (int32)MemberClass, *ItemDef->DisplayName.ToString());
        return EInventoryResult::FailedClassRestriction;
    }

    // Verify the item is currently in the inventory.
    if (!HasItem(ItemDef))
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] EquipItem: %s not in inventory."),
            *ItemDef->DisplayName.ToString());
        return EInventoryResult::FailedNotFound;
    }

    const EEquipmentSlot TargetSlot = ItemDef->EquipmentSlot;

    // Swap: if the slot is already occupied, return the old item to inventory.
    TSoftObjectPtr<UItemDefinition> CurrentlyEquipped = Member->EquipmentLoadout.GetSlot(TargetSlot);
    if (!CurrentlyEquipped.IsNull())
    {
        UItemDefinition* OldItem = CurrentlyEquipped.LoadSynchronous();
        if (OldItem)
        {
            // Return old item to inventory — bypass weight check since we're also
            // removing the new item's weight simultaneously.
            FInventoryEntry ReturnEntry;
            ReturnEntry.Definition        = OldItem;
            ReturnEntry.Quantity          = 1;
            ReturnEntry.AcquiredTimestamp = FDateTime::UtcNow();
            RuntimeInventory.Add(ReturnEntry);

            // Reverse old item modifiers.
            ApplyEquipmentModifiers(CharacterID, OldItem, false);
            UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] EquipItem: Swapped out %s from slot %d."),
                *OldItem->DisplayName.ToString(), (int32)TargetSlot);
        }
    }

    // Remove new item from inventory (weight exits the pool).
    RemoveItem(ItemDef, 1);

    // Write new item into the loadout slot.
    Member->EquipmentLoadout.SetSlot(TargetSlot, ItemDef);

    // Apply stat modifiers from the new item.
    ApplyEquipmentModifiers(CharacterID, ItemDef, true);

    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] EquipItem: %s equipped %s in slot %d."),
        *CharacterID.ToString(), *ItemDef->DisplayName.ToString(), (int32)TargetSlot);

    OnInventoryChanged.Broadcast();
    return EInventoryResult::Success;
}

// ---------------------------------------------------------------------------
// UnequipItem
// ---------------------------------------------------------------------------

EInventoryResult UInventorySubsystem::UnequipItem(const FGuid& CharacterID, EEquipmentSlot Slot)
{
    FPartyMemberData* Member = FindPartyMember(CharacterID);
    if (!Member) return EInventoryResult::FailedNotFound;

    TSoftObjectPtr<UItemDefinition> Equipped = Member->EquipmentLoadout.GetSlot(Slot);
    if (Equipped.IsNull()) return EInventoryResult::FailedNotFound;

    UItemDefinition* ItemDef = Equipped.LoadSynchronous();
    if (!ItemDef) return EInventoryResult::FailedNotFound;

    // Weight check: returning this item to inventory must not overflow.
    const float ReturnWeight = ItemDef->WeightPerUnit;
    if (GetCurrentWeight() + ReturnWeight > GetMaxWeight())
    {
        UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] UnequipItem: Cannot unequip %s — would exceed max weight."),
            *ItemDef->DisplayName.ToString());
        return EInventoryResult::FailedOverweight;
    }

    // Clear the slot first to avoid counting its weight twice during AddItem.
    Member->EquipmentLoadout.SetSlot(Slot, nullptr);

    // Reverse stat modifiers.
    ApplyEquipmentModifiers(CharacterID, ItemDef, false);

    // Return item to inventory.
    AddItem(ItemDef, 1);

    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] UnequipItem: %s returned %s to inventory."),
        *CharacterID.ToString(), *ItemDef->DisplayName.ToString());

    OnInventoryChanged.Broadcast();
    return EInventoryResult::Success;
}

// ---------------------------------------------------------------------------
// EquipConsumable / UnequipConsumable
// ---------------------------------------------------------------------------

EInventoryResult UInventorySubsystem::EquipConsumable(const FGuid& CharacterID, int32 SlotIndex, UItemDefinition* ItemDef)
{
    if (!ItemDef || SlotIndex < 0 || SlotIndex >= 4) return EInventoryResult::FailedNotFound;

    if (!ItemDef->IsCombatConsumable())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] EquipConsumable: %s is not a consumable."),
            *ItemDef->DisplayName.ToString());
        return EInventoryResult::FailedWrongCategory;
    }

    FPartyMemberData* Member = FindPartyMember(CharacterID);
    if (!Member) return EInventoryResult::FailedNotFound;

    if (!Member->EquipmentLoadout.CombatConsumables[SlotIndex].IsNull())
    {
        UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] EquipConsumable: Slot %d already occupied. Unequip first."), SlotIndex);
        return EInventoryResult::FailedSlotOccupied;
    }

    if (!HasItem(ItemDef)) return EInventoryResult::FailedNotFound;

    RemoveItem(ItemDef, 1);
    Member->EquipmentLoadout.CombatConsumables[SlotIndex] = ItemDef;

    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] EquipConsumable: %s → %s slot %d."),
        *ItemDef->DisplayName.ToString(), *CharacterID.ToString(), SlotIndex);

    OnInventoryChanged.Broadcast();
    return EInventoryResult::Success;
}

EInventoryResult UInventorySubsystem::UnequipConsumable(const FGuid& CharacterID, int32 SlotIndex)
{
    if (SlotIndex < 0 || SlotIndex >= 4) return EInventoryResult::FailedNotFound;

    FPartyMemberData* Member = FindPartyMember(CharacterID);
    if (!Member) return EInventoryResult::FailedNotFound;

    TSoftObjectPtr<UItemDefinition> Consumable = Member->EquipmentLoadout.CombatConsumables[SlotIndex];
    if (Consumable.IsNull()) return EInventoryResult::FailedNotFound;

    UItemDefinition* ItemDef = Consumable.LoadSynchronous();
    Member->EquipmentLoadout.CombatConsumables[SlotIndex] = nullptr;

    if (ItemDef) AddItem(ItemDef, 1);

    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] UnequipConsumable: Returned slot %d for %s."),
        SlotIndex, *CharacterID.ToString());

    OnInventoryChanged.Broadcast();
    return EInventoryResult::Success;
}

// ---------------------------------------------------------------------------
// DropItem
// ---------------------------------------------------------------------------

EInventoryResult UInventorySubsystem::DropItem(UItemDefinition* ItemDef, const FVector& SpawnLocation)
{
    if (!ItemDef) return EInventoryResult::FailedNotFound;

    if (ItemDef->bIsKeyItem)
    {
        UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] DropItem: %s is a key item and cannot be dropped."),
            *ItemDef->DisplayName.ToString());
        return EInventoryResult::FailedKeyItem;
    }

    EInventoryResult RemoveResult = RemoveItem(ItemDef, 1);
    if (RemoveResult != EInventoryResult::Success) return RemoveResult;

    SpawnWorldItem(ItemDef, SpawnLocation);

    return EInventoryResult::Success;
}

AWorldItemActor* UInventorySubsystem::SpawnWorldItem(UItemDefinition* ItemDef, const FVector& Location,
    const FRotator& Rotation)
{
    if (!ItemDef) return nullptr;

    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World) return nullptr;

    // Use ItemDefinition::WorldActorClass (the BP subclass with mesh assigned).
    // If not set, fall back to the base C++ class — actor will be invisible.
    TSubclassOf<AWorldItemActor> SpawnClass = nullptr;
    if (!ItemDef->WorldActorClass.IsNull())
    {
        SpawnClass = ItemDef->WorldActorClass.LoadSynchronous();
    }
    if (!SpawnClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[InventorySubsystem] SpawnWorldItem: '%s' has no WorldActorClass set — spawning base C++ class (no mesh). Assign WorldActorClass on the ItemDefinition DataAsset."),
            *ItemDef->DisplayName.ToString());
        SpawnClass = AWorldItemActor::StaticClass();
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AWorldItemActor* Spawned = World->SpawnActor<AWorldItemActor>(SpawnClass, Location, Rotation, Params);

    if (Spawned)
    {
        Spawned->InitializeAsDroppedItem(ItemDef);
        UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] SpawnWorldItem: spawned '%s' (%s) at %s."),
            *ItemDef->DisplayName.ToString(), *GetNameSafe(SpawnClass), *Location.ToString());
    }

    return Spawned;
}

// ---------------------------------------------------------------------------
// Weight
// ---------------------------------------------------------------------------

float UInventorySubsystem::GetMaxWeight() const
{
    UMyGameInstance* GI = GetGameInstance();
    if (!GI) return 10.f;

    const FPartyData& Party = GI->GetPartyData();
    int32 TotalStrength = 0;

    for (const FPartyMemberData& Member : Party.Members)
    {
        if (!Member.bIsActivePartyMember) continue;

        // Try to read Strength from the live ACharacterBase actor first
        // (most accurate, picks up any temporary bonuses applied mid-session).
        // Falls back to the Stats TMap persisted in FPartyMemberData if the
        // actor is not spawned yet (e.g., during save/load before BeginPlay).
        int32 MemberStr = 1;
        ACharacterBase* CharActor = FindCharacterActor(Member.CharacterID);
        if (CharActor && CharActor->GetUnitStats())
        {
            const FUnitAttributes& Attrs = CharActor->GetUnitStats()->Attributes;
            MemberStr = Attrs.Strength + CharActor->GetUnitStats()->TemporaryAttributeBonuses.Strength;
        }
        else if (Member.Stats.Contains(FName("Strength")))
        {
            MemberStr = Member.Stats[FName("Strength")];
        }

        TotalStrength += MemberStr;
    }

    // Formula: MaxWeight = 10 + 5 * SumSTR
    return 10.f + 5.f * static_cast<float>(TotalStrength);
}

float UInventorySubsystem::GetCurrentWeight() const
{
    float Total = 0.f;
    for (const FInventoryEntry& Entry : RuntimeInventory)
    {
        const UItemDefinition* Def = Entry.Definition.Get();
        if (Def) Total += Def->WeightPerUnit * Entry.Quantity;
    }
    return Total;
}

bool UInventorySubsystem::IsOverweight() const
{
    return GetCurrentWeight() > GetMaxWeight();
}

// ---------------------------------------------------------------------------
// Gold (party currency)
//
// All three methods route through PartyManagerSubsystem rather than writing
// to UMyGameInstance::PartyData directly. Same dual-storage rationale as
// FindMemberMutable / NotifyMemberMutated: PartyManagerSubsystem holds the
// authoritative runtime cache, and any mutation that bypasses it leaves the
// UI reading a stale value until the next SyncFromGameInstance.
// ---------------------------------------------------------------------------

int32 UInventorySubsystem::GetGold() const
{
    UMyGameInstance* GI = GetGameInstance();
    if (!GI) return 0;

    UPartyManagerSubsystem* PartyMgr = GI->GetSubsystem<UPartyManagerSubsystem>();
    if (!PartyMgr) return 0;

    return PartyMgr->GetPartyData().Gold;
}

void UInventorySubsystem::AddGold(int32 Amount)
{
    if (Amount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] AddGold: non-positive amount %d ignored."),
            Amount);
        return;
    }

    UMyGameInstance* GI = GetGameInstance();
    UPartyManagerSubsystem* PartyMgr = GI ? GI->GetSubsystem<UPartyManagerSubsystem>() : nullptr;
    if (!PartyMgr)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventorySubsystem] AddGold: PartyManagerSubsystem missing."));
        return;
    }

    // Mutate in-place via the runtime cache. PartyManagerSubsystem exposes a
    // mutable view through its existing API; we use it to avoid copying the
    // full FPartyData every call.
    FPartyData& Data = PartyMgr->GetPartyDataMutable();
    const int32 OldGold = Data.Gold;
    Data.Gold = FMath::Max(0, OldGold + Amount);

    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] AddGold: %d -> %d (+%d)."),
        OldGold, Data.Gold, Amount);

    PartyMgr->NotifyMemberMutated();
    OnGoldChanged.Broadcast(Data.Gold);
}

bool UInventorySubsystem::SpendGold(int32 Amount)
{
    if (Amount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] SpendGold: non-positive amount %d ignored."),
            Amount);
        return false;
    }

    UMyGameInstance* GI = GetGameInstance();
    UPartyManagerSubsystem* PartyMgr = GI ? GI->GetSubsystem<UPartyManagerSubsystem>() : nullptr;
    if (!PartyMgr)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventorySubsystem] SpendGold: PartyManagerSubsystem missing."));
        return false;
    }

    FPartyData& Data = PartyMgr->GetPartyDataMutable();
    if (Data.Gold < Amount)
    {
        UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] SpendGold: insufficient (have=%d, need=%d)."),
            Data.Gold, Amount);
        return false;
    }

    const int32 OldGold = Data.Gold;
    Data.Gold = OldGold - Amount;

    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] SpendGold: %d -> %d (-%d)."),
        OldGold, Data.Gold, Amount);

    PartyMgr->NotifyMemberMutated();
    OnGoldChanged.Broadcast(Data.Gold);
    return true;
}

// ---------------------------------------------------------------------------
// Sort
// ---------------------------------------------------------------------------

void UInventorySubsystem::SetSortLayers(const TArray<FInventorySortLayer>& NewLayers)
{
    // Clamp to maximum allowed layers.
    SortLayers = NewLayers;
    if (SortLayers.Num() > MaxSortLayers) SortLayers.SetNum(MaxSortLayers);

    ApplySort();
    OnInventoryChanged.Broadcast();
    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] Sort layers updated — %d layers active."), SortLayers.Num());
}

void UInventorySubsystem::ApplySort()
{
    if (SortLayers.IsEmpty()) return;

    // Stable multi-key sort: apply layers from last to first so that the first
    // layer ultimately dominates (each stable pass preserves previous order for ties).
    for (int32 LayerIdx = SortLayers.Num() - 1; LayerIdx >= 0; --LayerIdx)
    {
        const FInventorySortLayer& Layer = SortLayers[LayerIdx];
        if (Layer.Field == EInventorySortField::None) continue;

        const bool bAscending = (Layer.Order == EInventorySortOrder::Ascending);

        // StableSort preserves relative order of equal elements — critical for
        // multi-layer sorting to work correctly.
        Algo::StableSort(RuntimeInventory, [this, &Layer, bAscending]
            (const FInventoryEntry& A, const FInventoryEntry& B)
        {
            const int32 Cmp = CompareEntries(A, B, Layer.Field);
            return bAscending ? Cmp < 0 : Cmp > 0;
        });
    }
}

int32 UInventorySubsystem::CompareEntries(const FInventoryEntry& A, const FInventoryEntry& B, EInventorySortField Field) const
{
    const UItemDefinition* DefA = A.Definition.Get();
    const UItemDefinition* DefB = B.Definition.Get();

    // Null entries always sort to the end regardless of direction.
    if (!DefA && !DefB) return 0;
    if (!DefA) return 1;
    if (!DefB) return -1;

    switch (Field)
    {
    case EInventorySortField::Name:
        return DefA->DisplayName.CompareTo(DefB->DisplayName);

    case EInventorySortField::Category:
        return (int32)DefA->Category - (int32)DefB->Category;

    case EInventorySortField::DateAdded:
        if (A.AcquiredTimestamp < B.AcquiredTimestamp) return -1;
        if (A.AcquiredTimestamp > B.AcquiredTimestamp) return  1;
        return 0;

    case EInventorySortField::GoldValue:
        return DefA->GoldValue - DefB->GoldValue;

    case EInventorySortField::Weight:
        {
            const float WeightA = DefA->WeightPerUnit * A.Quantity;
            const float WeightB = DefB->WeightPerUnit * B.Quantity;
            if (WeightA < WeightB) return -1;
            if (WeightA > WeightB) return  1;
            return 0;
        }

    default:
        return 0;
    }
}

// ---------------------------------------------------------------------------
// Read access helpers
// ---------------------------------------------------------------------------

const FInventoryEntry* UInventorySubsystem::FindEntry(const UItemDefinition* ItemDef) const
{
    return RuntimeInventory.FindByPredicate([ItemDef](const FInventoryEntry& E)
    {
        return E.Definition.Get() == ItemDef;
    });
}

bool UInventorySubsystem::HasItem(UItemDefinition* ItemDef) const
{
    return FindEntry(ItemDef) != nullptr;
}

// ---------------------------------------------------------------------------
// IItemSource implementation
//
// All methods here are called from UItemContainerSubsystem::TransferItem.
// Stack-merging logic mirrors AddItem; the key difference is that
// ReceiveTransfer ignores the weight ceiling (see header comment).
// ---------------------------------------------------------------------------

bool UInventorySubsystem::HasItem(const UItemDefinition* ItemDef, int32 Quantity) const
{
    if (!ItemDef || Quantity <= 0) return false;

    // Currency lives in PartyData::Gold, not RuntimeInventory.
    if (ItemDef->Category == EItemCategory::Currency)
    {
        return GetGold() >= Quantity;
    }

    int32 Total = 0;
    for (const FInventoryEntry& Entry : RuntimeInventory)
    {
        if (Entry.Definition.Get() == ItemDef)
        {
            Total += Entry.Quantity;
            if (Total >= Quantity) return true;
        }
    }
    return false;
}

bool UInventorySubsystem::CanAcceptItem(const UItemDefinition* ItemDef, int32 Quantity) const
{
    if (!ItemDef || Quantity <= 0) return false;

    // Currency does not occupy slots — always acceptable.
    if (ItemDef->Category == EItemCategory::Currency) return true;

    int32 Remaining       = Quantity;
    const int32 MaxStack  = FMath::Max(1, ItemDef->MaxStackSize);

    // Pass 1: room in existing stacks.
    for (const FInventoryEntry& Entry : RuntimeInventory)
    {
        if (Entry.Definition.Get() != ItemDef) continue;
        const int32 Room = MaxStack - Entry.Quantity;
        if (Room > 0)
        {
            Remaining -= FMath::Min(Room, Remaining);
            if (Remaining <= 0) return true;
        }
    }

    // Pass 2: open new slots.
    int32 FreeSlots = MaxInventorySlots - RuntimeInventory.Num();
    while (Remaining > 0 && FreeSlots > 0)
    {
        Remaining -= FMath::Min(MaxStack, Remaining);
        --FreeSlots;
    }
    return Remaining <= 0;
}

EInventoryResult UInventorySubsystem::ReceiveTransfer(UItemDefinition* ItemDef, int32 Quantity)
{
    if (!ItemDef || Quantity <= 0) return EInventoryResult::FailedNotFound;

    // Currency interception: identical to AddItem. The container side already
    // removed its FInventoryEntry via ProvideTransfer; we just convert to gold
    // here and skip slot allocation. End result: chest "Gold x50" entry
    // disappears, player counter goes up by 50.
    if (ItemDef->Category == EItemCategory::Currency)
    {
        AddGold(Quantity);
        return EInventoryResult::Success;
    }

    // Stack-merge first, then open slots — same logic as AddItem but without
    // the weight gate. Allowing overweight here is intentional: the player
    // transferring from a chest can choose to overload, and the Overweight
    // status (applied by RecalculateWeightAndUpdateStatus) provides feedback.
    int32 Remaining       = Quantity;
    const int32 MaxStack  = FMath::Max(1, ItemDef->MaxStackSize);

    for (FInventoryEntry& Entry : RuntimeInventory)
    {
        if (Remaining <= 0) break;
        if (Entry.Definition.LoadSynchronous() != ItemDef) continue;
        const int32 Room = MaxStack - Entry.Quantity;
        if (Room <= 0) continue;
        const int32 Take = FMath::Min(Room, Remaining);
        Entry.Quantity += Take;
        Remaining      -= Take;
    }

    while (Remaining > 0)
    {
        if (RuntimeInventory.Num() >= MaxInventorySlots)
        {
            UE_LOG(LogTemp, Log,
                TEXT("[InventorySubsystem] ReceiveTransfer: full — %d unit(s) of '%s' rejected."),
                Remaining, *ItemDef->DisplayName.ToString());
            return EInventoryResult::FailedInventoryFull;
        }
        FInventoryEntry NewEntry;
        NewEntry.Definition        = ItemDef;
        NewEntry.Quantity          = FMath::Min(Remaining, MaxStack);
        NewEntry.AcquiredTimestamp = FDateTime::UtcNow();
        RuntimeInventory.Add(NewEntry);
        Remaining -= NewEntry.Quantity;
    }

    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] ReceiveTransfer: '%s' x%d accepted (slots=%d)."),
        *ItemDef->DisplayName.ToString(), Quantity, RuntimeInventory.Num());

    ApplySort();
    RecalculateWeightAndUpdateStatus();
    OnInventoryChanged.Broadcast();
    return EInventoryResult::Success;
}

EInventoryResult UInventorySubsystem::ProvideTransfer(UItemDefinition* ItemDef, int32 Quantity)
{
    // Currency interception: gold is not in RuntimeInventory, it's in
    // PartyData::Gold. Decrement that counter directly. SpendGold reports
    // failure if the player doesn't have enough.
    if (ItemDef && ItemDef->Category == EItemCategory::Currency)
    {
        return SpendGold(Quantity)
            ? EInventoryResult::Success
            : EInventoryResult::FailedNotFound;
    }

    // Outgoing transfer reuses the existing weight-aware RemoveItem.
    return RemoveItem(ItemDef, Quantity);
}

FText UInventorySubsystem::GetSourceDisplayName() const
{
    return NSLOCTEXT("Inventory", "PlayerInventoryLabel", "Inventory");
}

FEquipmentLoadout* UInventorySubsystem::GetLoadoutForCharacter(const FGuid& CharacterID)
{
    FPartyMemberData* Member = FindPartyMember(CharacterID);
    return Member ? &Member->EquipmentLoadout : nullptr;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UInventorySubsystem::RecalculateWeightAndUpdateStatus()
{
    const float CurrentWeight = GetCurrentWeight();
    const float MaxWeight     = GetMaxWeight();
    const bool  bNowOverweight = CurrentWeight > MaxWeight;

    OnWeightChanged.Broadcast(CurrentWeight);

    if (bNowOverweight == bWasOverweight) return;
    bWasOverweight = bNowOverweight;

    // Apply or remove the Overweight status on all active party members.
    // The actual status application routes through StatusEffectManager (battle)
    // or is tagged directly onto the actor (exploration); the Blueprint layer
    // handles both contexts by listening to OnOverweightChanged.
    UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] Overweight state changed → %s. Weight: %.1f / %.1f."),
        bNowOverweight ? TEXT("OVERWEIGHT") : TEXT("OK"), CurrentWeight, MaxWeight);

    // Broadcast a dedicated delegate so the game mode / BP can apply the status.
    // We intentionally do NOT call StatusEffectManager here to keep this subsystem
    // decoupled from the combat module — StatusEffectManager is battle-only.
    // TODO: hook this delegate in ExplorationPlayerController and BattlePreparationController.
    if (bNowOverweight)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventorySubsystem] Party is overweight — Overweight status should be applied."));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] Party is no longer overweight — Overweight status should be removed."));
    }
}

void UInventorySubsystem::ApplyEquipmentModifiers(const FGuid& CharacterID, const UItemDefinition* ItemDef, bool bApply)
{
    if (!ItemDef || ItemDef->StatModifiers.IsEmpty()) return;

    ACharacterBase* CharActor = FindCharacterActor(CharacterID);
    if (!CharActor) return;

    UUnitStatsComponent* Stats = CharActor->GetUnitStats();
    if (!Stats) return;

    for (const FItemStatModifier& Mod : ItemDef->StatModifiers)
    {
        if (!Mod.bModifiesAttribute) continue;

        const int32 Delta = bApply ? Mod.FlatBonus : -Mod.FlatBonus;

        // Apply delta to TemporaryAttributeBonuses — same field used by status effects,
        // so the existing RecalculateBattleStats pipeline picks it up automatically.
        switch (Mod.AttributeTarget)
        {
        case EUnitAttribute::Constitution:  Stats->TemporaryAttributeBonuses.Constitution  += Delta; break;
        case EUnitAttribute::Strength:      Stats->TemporaryAttributeBonuses.Strength      += Delta; break;
        case EUnitAttribute::Dexterity:     Stats->TemporaryAttributeBonuses.Dexterity     += Delta; break;
        case EUnitAttribute::Knowledge:     Stats->TemporaryAttributeBonuses.Knowledge     += Delta; break;
        case EUnitAttribute::Focus:         Stats->TemporaryAttributeBonuses.Focus         += Delta; break;
        case EUnitAttribute::Technique:     Stats->TemporaryAttributeBonuses.Technique     += Delta; break;
        case EUnitAttribute::Evasiveness:   Stats->TemporaryAttributeBonuses.Evasiveness   += Delta; break;
        case EUnitAttribute::Speed:         Stats->TemporaryAttributeBonuses.Speed         += Delta; break;
        default: break;
        }

        UE_LOG(LogTemp, Log, TEXT("[InventorySubsystem] ApplyEquipmentModifiers: %s %s attr %d by %d."),
            bApply ? TEXT("Applied") : TEXT("Reversed"),
            *ItemDef->DisplayName.ToString(),
            (int32)Mod.AttributeTarget, FMath::Abs(Delta));
    }

    // If STR changed, recalculate MaxWeight and re-check overweight state.
    RecalculateWeightAndUpdateStatus();
}

ACharacterBase* UInventorySubsystem::FindCharacterActor(const FGuid& CharacterID) const
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World) return nullptr;

    // CharacterID is the FGuid assigned to a FPartyMemberData entry — it is NOT
    // the same as the SaveableComponent identifier (actor FName for placed actors,
    // runtime GUID string for spawned ones).
    // Locate the character by matching its CharacterClass from the party data,
    // since the CharacterID is not carried on the actor itself.
    const FPartyMemberData* MemberData = FindPartyMember(CharacterID);
    if (!MemberData || !MemberData->CharacterClass) return nullptr;

    for (TActorIterator<ACharacterBase> It(World); It; ++It)
    {
        ACharacterBase* Char = *It;
        if (Char && Char->GetClass() == MemberData->CharacterClass)
        {
            return Char;
        }
    }
    return nullptr;
}

FPartyMemberData* UInventorySubsystem::FindPartyMember(const FGuid& CharacterID) const
{
    UMyGameInstance* GI = GetGameInstance();
    if (!GI) return nullptr;

    // GetPartyData returns const ref; we need mutable access via SetPartyData pattern.
    // For now, cast away const — this is acceptable since GI owns the data.
    FPartyData& Party = const_cast<FPartyData&>(GI->GetPartyData());
    return Party.Members.FindByPredicate([&CharacterID](const FPartyMemberData& M)
    {
        return M.CharacterID == CharacterID;
    });
}

UMyGameInstance* UInventorySubsystem::GetGameInstance() const
{
    return Cast<UMyGameInstance>(GetOuter());
}
