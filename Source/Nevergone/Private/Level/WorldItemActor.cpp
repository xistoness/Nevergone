// Copyright Xyzto Works

#include "Level/WorldItemActor.h"

#include "ActorComponents/CombatPresenceComponent.h"
#include "ActorComponents/InteractableComponent.h"
#include "ActorComponents/SaveableComponent.h"
#include "Data/ActorSaveData.h"
#include "Data/ItemDefinition.h"
#include "EngineTools/CollisionChannels.h"
#include "World/InventorySubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

const FName AWorldItemActor::SaveKey_ItemPath        = FName("ItemPath");
const FName AWorldItemActor::SaveKey_VisitsRemaining = FName("VisitsRemaining");

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AWorldItemActor::AWorldItemActor()
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

    PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
    PickupMesh->SetupAttachment(RootComponent);
    PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Nameplate = CreateDefaultSubobject<UWidgetComponent>(TEXT("Nameplate"));
    Nameplate->SetupAttachment(RootComponent);
    Nameplate->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
    Nameplate->SetWidgetSpace(EWidgetSpace::Screen);
    Nameplate->SetDrawSize(FVector2D(200.f, 40.f));

    InteractableComponent = CreateDefaultSubobject<UInteractableComponent>(TEXT("InteractableComponent"));

    // Level-placed items: bPersistAcrossLoads = false.
    //   CollectWorldSaveData saves their state while alive.
    //   SpawnMissingActors never respawns them (CDO check).
    //   RestoreWorldState destroys them if absent from the save (discrepancy check).
    //
    // Dropped items: bPersistAcrossLoads is flipped to true by InitializeAsDroppedItem
    //   so SpawnMissingActors respawns them until their visit counter expires.
    SaveableComponent = CreateDefaultSubobject<USaveableComponent>(TEXT("SaveableComponent"));
    SaveableComponent->bPersistAcrossLoads = false;

    // --- Combat presence ---
    // World items remain on the ground in combat as flavor. They do not block
    // tiles (their interaction sphere ignores the visibility channel that the
    // grid generator traces with), so leaving them visible has no tactical
    // impact. Marking as Ignore is more honest than BecomeObstacle here —
    // they are not obstacles.
    auto* CombatPresence = CreateDefaultSubobject<UCombatPresenceComponent>(TEXT("CombatPresenceComponent"));
    CombatPresence->DefaultPolicy = ECombatPresencePolicy::Ignore;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void AWorldItemActor::BeginPlay()
{
    Super::BeginPlay();
    RefreshVisuals();
}

// ---------------------------------------------------------------------------
// Item setup
// ---------------------------------------------------------------------------

void AWorldItemActor::SetItemDefinition(UItemDefinition* InItemDef)
{
    if (!InItemDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WorldItemActor] SetItemDefinition: called with null."));
        return;
    }

    ItemDefinition = InItemDef;
    RefreshVisuals();

    UE_LOG(LogTemp, Log, TEXT("[WorldItemActor] ItemDefinition set to '%s'."),
        *InItemDef->DisplayName.ToString());
}

UItemDefinition* AWorldItemActor::GetItemDefinition() const
{
    return ItemDefinition.Get();
}

void AWorldItemActor::InitializeAsDroppedItem(UItemDefinition* InItemDef)
{
    if (!InItemDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WorldItemActor] InitializeAsDroppedItem: called with null."));
        return;
    }

    VisitsRemaining = InItemDef->DropVisitLifetime;

    // Dropped items must persist across loads so SpawnMissingActors can respawn them.
    // This overrides the default false set in the constructor for level-placed items.
    if (SaveableComponent)
    {
        SaveableComponent->bPersistAcrossLoads = true;
    }

    SetItemDefinition(InItemDef);

    UE_LOG(LogTemp, Log, TEXT("[WorldItemActor] InitializeAsDroppedItem: '%s', VisitsRemaining=%d."),
        *InItemDef->DisplayName.ToString(), VisitsRemaining);
}

void AWorldItemActor::RefreshVisuals()
{
    UItemDefinition* Def = ItemDefinition.Get();
    if (!Def) return;
    BP_OnVisualsReady(Def);
}

// ---------------------------------------------------------------------------
// IInteractable
// ---------------------------------------------------------------------------

void AWorldItemActor::Interact_Implementation(AActor* Interactor)
{
    UE_LOG(LogTemp, Log, TEXT("[WorldItemActor] Interact called by '%s'."),
        Interactor ? *Interactor->GetName() : TEXT("null"));
    PickUp(Interactor);
}

void AWorldItemActor::PickUp(AActor* Interactor)
{
    UItemDefinition* Def = ItemDefinition.LoadSynchronous();
    if (!Def)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WorldItemActor] PickUp: ItemDefinition could not be loaded."));
        Destroy();
        return;
    }

    UInventorySubsystem* Inventory = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UInventorySubsystem>()
        : nullptr;

    if (!Inventory)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WorldItemActor] PickUp: InventorySubsystem not found."));
        return;
    }

    const EInventoryResult Result = Inventory->AddItem(Def, 1);
    if (Result == EInventoryResult::Success)
    {
        UE_LOG(LogTemp, Log, TEXT("[WorldItemActor] PickUp: '%s' added to inventory. Destroying actor."),
            *Def->DisplayName.ToString());

        // Destroy immediately. The save system (CollectWorldSaveData) only iterates
        // live actors, so this actor will be absent from the save — the discrepancy
        // check in RestoreSaveableActors will destroy it again on the next load.
        // No need to keep the actor alive or set a "collected" flag.
        Destroy();
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[WorldItemActor] PickUp: '%s' rejected by inventory — Result=%d."),
            *Def->DisplayName.ToString(), (int32)Result);
        BP_OnPickupFailed(Result);
    }
}

// ---------------------------------------------------------------------------
// ISaveParticipant
// ---------------------------------------------------------------------------

void AWorldItemActor::WriteSaveData_Implementation(FActorSaveData& OutData) const
{
    // --- ItemPath ---
    const FString PathStr = ItemDefinition.IsNull()
        ? FString()
        : ItemDefinition.ToSoftObjectPath().ToString();
    {
        TArray<uint8> Bytes;
        FMemoryWriter Writer(Bytes, true);
        FObjectAndNameAsStringProxyArchive Proxy(Writer, false);
        Proxy << const_cast<FString&>(PathStr);
        FSavePayload Payload;
        Payload.Data = Bytes;
        OutData.CustomDataMap.Add(SaveKey_ItemPath, Payload);
    }

    // --- VisitsRemaining ---
    // Save the current value without decrementing. Decrement happens in ReadSaveData
    // when the actor is restored on a new visit — that is the moment the "visit" occurs.
    // Level-placed items use VisitsRemaining = -1 and are never decremented.
    {
        TArray<uint8> Bytes;
        FMemoryWriter Writer(Bytes, true);
        FObjectAndNameAsStringProxyArchive Proxy(Writer, false);
        int32 TempVisitsRemaining = VisitsRemaining;
        Proxy << TempVisitsRemaining;
        FSavePayload Payload;
        Payload.Data = Bytes;
        OutData.CustomDataMap.Add(SaveKey_VisitsRemaining, Payload);
    }

    UE_LOG(LogTemp, Log, TEXT("[WorldItemActor] WriteSaveData: '%s' visits=%d."),
        *PathStr, VisitsRemaining);
}

void AWorldItemActor::ReadSaveData_Implementation(const FActorSaveData& InData)
{
    if (const FSavePayload* PathPayload = InData.CustomDataMap.Find(SaveKey_ItemPath))
    {
        FString PathStr;
        FMemoryReader Reader(PathPayload->Data, true);
        FObjectAndNameAsStringProxyArchive Proxy(Reader, false);
        Proxy << PathStr;
        if (!PathStr.IsEmpty())
        {
            ItemDefinition = TSoftObjectPtr<UItemDefinition>(FSoftObjectPath(PathStr));
        }
    }

    if (const FSavePayload* VisitsPayload = InData.CustomDataMap.Find(SaveKey_VisitsRemaining))
    {
        FMemoryReader Reader(VisitsPayload->Data, true);
        FObjectAndNameAsStringProxyArchive Proxy(Reader, false);
        Proxy << VisitsRemaining;
    }

    // Decrement on restore: each load counts as one visit consumed.
    // Level-placed items use VisitsRemaining = -1 and are never decremented.
    // When visits reach 0, flag the actor so the next CollectWorldSaveData
    // writes bPersistAcrossLoads = false, preventing a future respawn.
    if (VisitsRemaining > 0)
    {
        VisitsRemaining--;
        UE_LOG(LogTemp, Log, TEXT("[WorldItemActor] ReadSaveData: '%s' visit consumed, remaining=%d."),
            ItemDefinition.IsNull() ? TEXT("null") : *ItemDefinition.GetAssetName(),
            VisitsRemaining);

        if (VisitsRemaining == 0 && SaveableComponent)
        {
            SaveableComponent->bPersistAcrossLoads = false;
            UE_LOG(LogTemp, Log, TEXT("[WorldItemActor] ReadSaveData: '%s' visits exhausted — will not respawn after next save."),
                ItemDefinition.IsNull() ? TEXT("null") : *ItemDefinition.GetAssetName());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[WorldItemActor] ReadSaveData: '%s' visits=%d."),
        ItemDefinition.IsNull() ? TEXT("null") : *ItemDefinition.GetAssetName(),
        VisitsRemaining);
}

void AWorldItemActor::OnPostRestore_Implementation()
{
    RefreshVisuals();

    UE_LOG(LogTemp, Log, TEXT("[WorldItemActor] OnPostRestore: '%s' visits=%d."),
        ItemDefinition.IsNull() ? TEXT("unknown") : *ItemDefinition.GetAssetName(),
        VisitsRemaining);
}
