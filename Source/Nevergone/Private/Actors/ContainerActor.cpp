// Copyright Xyzto Works

#include "Actors/ContainerActor.h"
#include "ActorComponents/CombatPresenceComponent.h"
#include "Data/ContainerDefinition.h"
#include "Data/LootTable.h"
#include "Data/ItemDefinition.h"
#include "Data/ActorSaveData.h"
#include "Interfaces/Interactable.h"
#include "ActorComponents/InteractableComponent.h"
#include "ActorComponents/SaveableComponent.h"
#include "EngineTools/CollisionChannels.h"
#include "World/InventorySubsystem.h" // For EInventoryResult enum definition
#include "Characters/PlayerControllers/ExplorationPlayerController.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#include "GameInstance/SaveKeys.h"

// ===========================================================================
// Lifecycle
// ===========================================================================

AContainerActor::AContainerActor()
{
    PrimaryActorTick.bCanEverTick = false;

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetSphereRadius(80.f);

    // Collision setup:
    // - QueryAndPhysics so overlap events fire and line traces register hits.
    // - WorldDynamic object type matches interaction trace filters.
    // - Visibility / Camera / GridTrace ignored to avoid false positives.
    // - Interactable channel blocked so interaction line traces hit this sphere.
    // - All other channels overlapped.
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility,        ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Camera,            ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(NevergoneCollision::GridTrace,    ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(NevergoneCollision::Interactable, ECR_Block);
    RootComponent = InteractionSphere;

    ContainerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ContainerMesh"));
    ContainerMesh->SetupAttachment(RootComponent);
    ContainerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    InteractableComponent = CreateDefaultSubobject<UInteractableComponent>(TEXT("InteractableComponent"));
    SaveableComponent     = CreateDefaultSubobject<USaveableComponent>(TEXT("SaveableComponent"));

    // Level-placed actor: state is saved while alive (Items, bHasBeenLooted)
    // but the actor itself is NEVER respawned by SpawnMissingActors — it lives in
    // the level package and is loaded by the engine. Setting this to false prevents
    // duplicate instances from being spawned by the save system if the world-actor
    // matching ever fails.
    SaveableComponent->bPersistAcrossLoads = false;

    // --- Combat presence ---
    // Containers stay visible in combat as scenery. Interaction is implicitly
    // disabled (the exploration interaction trace is dormant while the
    // ExplorationModeComponent is not ticking). BecomeObstacle is the
    // semantic policy; the cover system pass (Phase 3) will inspect this
    // policy when populating tactical cover info.
    auto* CombatPresence = CreateDefaultSubobject<UCombatPresenceComponent>(TEXT("CombatPresenceComponent"));
    CombatPresence->DefaultPolicy = ECombatPresencePolicy::BecomeObstacle;
}

void AContainerActor::BeginPlay()
{
    Super::BeginPlay();

    if (!Definition)
    {
        UE_LOG(LogTemp, Error, TEXT("[ContainerActor] '%s' has no Definition assigned — interactions will fail."),
            *GetName());
    }
}

// ===========================================================================
// IInteractable
// ===========================================================================

void AContainerActor::Interact_Implementation(AActor* Interactor)
{
    UE_LOG(LogTemp, Log, TEXT("[ContainerActor] '%s' interact triggered by '%s'."),
        *GetName(), *GetNameSafe(Interactor));

    if (!Definition)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ContainerActor] '%s' has no Definition — ignoring interact."),
            *GetName());
        return;
    }

    EnsureInitialized();

    // Hand off to the player controller, which owns the widget instance and
    // input mode transitions (mirrors the Inventory / PartyManager pattern).
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    AExplorationPlayerController* ExpPC = Cast<AExplorationPlayerController>(PC);
    if (!ExpPC)
    {
        UE_LOG(LogTemp, Error, TEXT("[ContainerActor] '%s' interact: no AExplorationPlayerController."),
            *GetName());
        return;
    }

    ExpPC->OpenContainerInteraction(this);
    OnContainerOpen();
}

void AContainerActor::OnContainerOpen()
{
    BP_ContainerOpen();
}

void AContainerActor::OnContainerClose()
{
    BP_ContainerClose();
}

// ===========================================================================
// First-open initialization
// ===========================================================================

void AContainerActor::EnsureInitialized()
{
    if (bHasBeenLooted)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[ContainerActor] '%s' already looted — skipping roll."),
            *GetName());
        return;
    }

    if (Definition && !Definition->LootTable.IsNull())
    {
        ULootTable* Table = Definition->LootTable.LoadSynchronous();
        if (Table)
        {
            const TArray<FRolledLoot> Rolled = Table->Roll();
            for (const FRolledLoot& Drop : Rolled)
            {
                if (!Drop.ItemDef || Drop.Quantity <= 0) continue;
                AddInternal(Drop.ItemDef, Drop.Quantity);
            }
            UE_LOG(LogTemp, Log, TEXT("[ContainerActor] '%s' first-open roll produced %d entries."),
                *GetName(), Items.Num());
        }
    }

    bHasBeenLooted = true;
    OnContentsChanged.Broadcast();
}

// ===========================================================================
// IItemSource
// ===========================================================================

int32 AContainerActor::GetMaxSlots() const
{
    return Definition ? Definition->MaxSlots : 0;
}

bool AContainerActor::HasItem(const UItemDefinition* ItemDef, int32 Quantity) const
{
    if (!ItemDef || Quantity <= 0) return false;

    int32 Total = 0;
    for (const FInventoryEntry& Entry : Items)
    {
        if (Entry.Definition.LoadSynchronous() == ItemDef)
        {
            Total += Entry.Quantity;
            if (Total >= Quantity) return true;
        }
    }
    return false;
}

bool AContainerActor::CanAcceptItem(const UItemDefinition* ItemDef, int32 Quantity) const
{
    if (!ItemDef || Quantity <= 0) return false;

    // Mirror the AddInternal stack-merging logic in dry-run mode.
    int32 Remaining = Quantity;
    const int32 MaxStack = FMath::Max(1, ItemDef->MaxStackSize);

    // Pass 1: room in existing stacks
    for (const FInventoryEntry& Entry : Items)
    {
        if (Entry.Definition.LoadSynchronous() != ItemDef) continue;
        const int32 Room = MaxStack - Entry.Quantity;
        if (Room > 0)
        {
            Remaining -= FMath::Min(Room, Remaining);
            if (Remaining <= 0) return true;
        }
    }

    // Pass 2: open new slots
    int32 FreeSlots = GetMaxSlots() - Items.Num();
    while (Remaining > 0 && FreeSlots > 0)
    {
        Remaining -= FMath::Min(MaxStack, Remaining);
        --FreeSlots;
    }

    return Remaining <= 0;
}

EInventoryResult AContainerActor::ReceiveTransfer(UItemDefinition* ItemDef, int32 Quantity)
{
    const EInventoryResult Result = AddInternal(ItemDef, Quantity);
    if (Result == EInventoryResult::Success)
    {
        UE_LOG(LogTemp, Log, TEXT("[ContainerActor] '%s' ReceiveTransfer: '%s' x%d. UsedSlots=%d."),
            *GetName(),
            *ItemDef->DisplayName.ToString(),
            Quantity,
            Items.Num());
        OnContentsChanged.Broadcast();
    }
    return Result;
}

EInventoryResult AContainerActor::ProvideTransfer(UItemDefinition* ItemDef, int32 Quantity)
{
    if (!ItemDef || Quantity <= 0) return EInventoryResult::FailedNotFound;

    int32 Remaining = Quantity;
    for (int32 i = Items.Num() - 1; i >= 0 && Remaining > 0; --i)
    {
        FInventoryEntry& Entry = Items[i];
        if (Entry.Definition.LoadSynchronous() != ItemDef) continue;

        const int32 Take = FMath::Min(Entry.Quantity, Remaining);
        Entry.Quantity -= Take;
        Remaining      -= Take;

        if (Entry.Quantity <= 0)
        {
            Items.RemoveAt(i);
        }
    }

    if (Remaining > 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ContainerActor] '%s' ProvideTransfer '%s' x%d: only %d available."),
            *GetName(),
            *ItemDef->DisplayName.ToString(),
            Quantity,
            Quantity - Remaining);
        return EInventoryResult::FailedNotFound;
    }

    UE_LOG(LogTemp, Log, TEXT("[ContainerActor] '%s' ProvideTransfer: '%s' x%d. UsedSlots=%d."),
        *GetName(),
        *ItemDef->DisplayName.ToString(),
        Quantity,
        Items.Num());

    OnContentsChanged.Broadcast();
    return EInventoryResult::Success;
}

FText AContainerActor::GetSourceDisplayName() const
{
    if (Definition && !Definition->DisplayName.IsEmpty())
    {
        return Definition->DisplayName;
    }

    // Fallback used only when a level was placed without a Definition asset —
    // legitimate gameplay never hits this branch, but we still surface a
    // localizable header rather than the raw actor name (which may include
    // engine-generated suffixes like "_C_42").
    UE_LOG(LogTemp, Warning,
        TEXT("[ContainerActor] GetSourceDisplayName: actor '%s' has no Definition — using fallback label"),
        *GetName());

    return NSLOCTEXT("Container", "MissingDefinitionFallback", "Container");
}

EInventoryResult AContainerActor::AddInternal(UItemDefinition* ItemDef, int32 Quantity)
{
    if (!ItemDef || Quantity <= 0)
    {
        return EInventoryResult::FailedNotFound;
    }

    int32 Remaining       = Quantity;
    const int32 MaxStack  = FMath::Max(1, ItemDef->MaxStackSize);

    // Pass 1: top up existing stacks.
    for (FInventoryEntry& Entry : Items)
    {
        if (Entry.Definition.LoadSynchronous() != ItemDef) continue;
        const int32 Room = MaxStack - Entry.Quantity;
        if (Room <= 0) continue;

        const int32 Take = FMath::Min(Room, Remaining);
        Entry.Quantity += Take;
        Remaining      -= Take;
        if (Remaining <= 0) break;
    }

    // Pass 2: open new slots.
    while (Remaining > 0)
    {
        if (Items.Num() >= GetMaxSlots())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[ContainerActor] '%s' AddInternal: ran out of slots, %d unit(s) of '%s' rejected."),
                *GetName(), Remaining, *ItemDef->DisplayName.ToString());
            return EInventoryResult::FailedInventoryFull;
        }
        FInventoryEntry NewEntry;
        NewEntry.Definition        = ItemDef;
        NewEntry.Quantity          = FMath::Min(MaxStack, Remaining);
        NewEntry.AcquiredTimestamp = FDateTime::UtcNow();
        Items.Add(NewEntry);
        Remaining -= NewEntry.Quantity;
    }

    return EInventoryResult::Success;
}

// ===========================================================================
// Read access
// ===========================================================================

float AContainerActor::GetTotalWeight() const
{
    float Total = 0.f;
    for (const FInventoryEntry& Entry : Items)
    {
        const UItemDefinition* Def = Entry.Definition.LoadSynchronous();
        if (!Def) continue;
        Total += Def->WeightPerUnit * (float)Entry.Quantity;
    }
    return Total;
}

// ===========================================================================
// ISaveParticipant
// ===========================================================================

void AContainerActor::WriteSaveData_Implementation(FActorSaveData& OutData) const
{
    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes, true);
    FObjectAndNameAsStringProxyArchive Proxy(Writer, false);

    // Manual per-field serialization. FArchive does NOT have a built-in
    // operator<< for FInventoryEntry (USTRUCT), so we serialize each field
    // explicitly. Same pattern WorldItemActor uses for its primitives.
    //
    // Order: count, then for each entry [path-string, quantity, ticks].
    // The DateTime is written as raw int64 ticks (FDateTime exposes GetTicks /
    // ctor-from-ticks) to dodge the same operator<< issue.
    int32 NumItems = Items.Num();
    Proxy << NumItems;

    for (const FInventoryEntry& Entry : Items)
    {
        FString PathStr = Entry.Definition.IsNull()
            ? FString()
            : Entry.Definition.ToSoftObjectPath().ToString();

        int32  TempQuantity  = Entry.Quantity;
        int64  TempTicks     = Entry.AcquiredTimestamp.GetTicks();

        Proxy << PathStr;
        Proxy << TempQuantity;
        Proxy << TempTicks;
    }

    bool TempLooted = bHasBeenLooted;
    Proxy << TempLooted;

    FSavePayload Payload;
    Payload.Data = MoveTemp(Bytes);
    OutData.CustomDataMap.Add(SaveKeys::Container, MoveTemp(Payload));

    UE_LOG(LogTemp, Log, TEXT("[ContainerActor] '%s' WriteSaveData: %d entries, looted=%s."),
        *GetName(), Items.Num(), bHasBeenLooted ? TEXT("true") : TEXT("false"));
}

void AContainerActor::ReadSaveData_Implementation(const FActorSaveData& InData)
{
    const FSavePayload* Payload = InData.CustomDataMap.Find(SaveKeys::Container);
    if (!Payload)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ContainerActor] '%s' ReadSaveData: no payload for key '%s' — keeping defaults (will re-roll loot on first open)."),
            *GetName(), *SaveKeys::Container.ToString());
        return;
    }

    FMemoryReader Reader(Payload->Data, true);
    FObjectAndNameAsStringProxyArchive Proxy(Reader, false);

    Items.Reset();

    int32 NumItems = 0;
    Proxy << NumItems;

    Items.Reserve(NumItems);
    for (int32 i = 0; i < NumItems; ++i)
    {
        FString PathStr;
        int32   TempQuantity = 0;
        int64   TempTicks    = 0;

        Proxy << PathStr;
        Proxy << TempQuantity;
        Proxy << TempTicks;

        FInventoryEntry NewEntry;
        if (!PathStr.IsEmpty())
        {
            NewEntry.Definition = TSoftObjectPtr<UItemDefinition>(FSoftObjectPath(PathStr));
        }
        NewEntry.Quantity          = TempQuantity;
        NewEntry.AcquiredTimestamp = FDateTime(TempTicks);
        Items.Add(MoveTemp(NewEntry));
    }

    Proxy << bHasBeenLooted;

    UE_LOG(LogTemp, Log, TEXT("[ContainerActor] '%s' ReadSaveData: %d entries, looted=%s."),
        *GetName(), Items.Num(), bHasBeenLooted ? TEXT("true") : TEXT("false"));
}

void AContainerActor::OnPostRestore_Implementation()
{
    // No visual state to refresh yet — closed/open animation, lid sprite, etc.
    // Hook reserved for when art adds them.
    OnContentsChanged.Broadcast();
}