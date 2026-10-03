// Copyright Xyzto Works

#include "Actors/Door.h"

#include "ActorComponents/CombatPresenceComponent.h"
#include "ActorComponents/InteractableComponent.h"
#include "ActorComponents/SaveableComponent.h"
#include "Characters/PlayerControllers/ExplorationPlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Data/ActorSaveData.h"
#include "Data/ItemDefinition.h"
#include "Data/NoiseDefinition.h"
#include "GameInstance/SaveKeys.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "TimerManager.h"
#include "Widgets/ExplorationPromptWidget.h"
#include "World/ExcursionNoiseSubsystem.h"
#include "World/InventorySubsystem.h"

const FName ADoor::SaveKey = SaveKeys::Door;

// ===========================================================================
// Construction
// ===========================================================================

ADoor::ADoor()
{
    PrimaryActorTick.bCanEverTick = false;

    // Plain SceneComponent root keeps the actor pivot independent from the
    // mesh pivot. When skeletal animation arrives, the door leaf can pivot
    // around its hinge socket without forcing the whole actor to rotate.
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // Visual-only mesh. A USkeletalMeshComponent has no collision bodies
    // without a Physics Asset, and we do not want to require one for every
    // door — DoorCollisionBox handles all blocking. Forcing NoCollision here
    // also prevents accidental misconfiguration in BP subclasses.
    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
    DoorMesh->SetupAttachment(Root);
    DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Actual blocking volume. Default extent is a placeholder; the BP subclass
    // tunes BoxExtent and RelativeLocation per door asset (e.g. half-thickness
    // on X, half-width on Y, half-height on Z, lifted by Z so the bottom sits
    // at the actor pivot).
    //
    // Standard UE door dimensions (~1m wide, ~2m tall, ~10cm thick):
    //   Extents = (5, 45, 100)  →  10x90x200 cm full size
    DoorCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorCollisionBox"));
    DoorCollisionBox->SetupAttachment(Root);
    DoorCollisionBox->SetBoxExtent(FVector(5.f, 45.f, 100.f));
    DoorCollisionBox->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
    DoorCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    DoorCollisionBox->SetCollisionObjectType(ECC_WorldStatic);
    DoorCollisionBox->SetCollisionResponseToAllChannels(ECR_Block);
    // The Interactable channel will be promoted to Block by
    // InteractableComponent::OnRegister regardless — that is fine. The
    // sphere's only purpose then is widening the interaction radius beyond
    // the visual door footprint.

    // Interaction trigger volume — wider than the box so the player can
    // interact from a small distance, similar to AContainerActor / AWorldItemActor.
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(Root);
    InteractionSphere->SetSphereRadius(110.f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);

    InteractableComponent = CreateDefaultSubobject<UInteractableComponent>(TEXT("InteractableComponent"));
    SaveableComponent     = CreateDefaultSubobject<USaveableComponent>(TEXT("SaveableComponent"));

    // Level-placed actor: state is saved while alive (bIsUnlocked) but the
    // actor itself is loaded from the level package — the save system must NOT
    // respawn it on load. Same pattern as AContainerActor.
    SaveableComponent->bPersistAcrossLoads = false;

    // Sensible default for the locked-prompt fallback.
    LockedPromptText = NSLOCTEXT("Door", "DefaultLockedPrompt", "It's locked.");

    // --- Combat presence ---
    // Doors remain in place during combat — they are scenery and a real
    // collision blocker. Interaction is implicitly disabled while the
    // ExplorationModeComponent is dormant.
    auto* CombatPresence = CreateDefaultSubobject<UCombatPresenceComponent>(TEXT("CombatPresenceComponent"));
    CombatPresence->DefaultPolicy = ECombatPresencePolicy::BecomeObstacle;
}

// ===========================================================================
// Lifecycle
// ===========================================================================

void ADoor::BeginPlay()
{
    Super::BeginPlay();

    // State authority: OnPostRestore runs BEFORE BeginPlay in this project
    // (driven by FWorldDelegates::OnWorldInitializedActors). On a first visit,
    // OnPostRestore initializes CurrentState from bStartLocked. On a revisit,
    // it initializes from the persisted bIsUnlocked flag. Either way, by the
    // time BeginPlay fires, CurrentState and bIsUnlocked are already correct
    // and BeginPlay must NOT overwrite them.
    //
    // Validation-only logic goes here.

    if (bStartLocked && !bIsUnlocked && RequiredKey.IsNull())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Door] '%s' starts Locked but has no RequiredKey assigned — only scripted unlock paths will work."),
            *GetName());
    }

    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' BeginPlay: state=%s bIsUnlocked=%d"),
        *GetName(),
        *UEnum::GetValueAsString(CurrentState),
        (int32)bIsUnlocked);
}

// ===========================================================================
// IInteractable
// ===========================================================================

void ADoor::Interact_Implementation(AActor* Interactor)
{
    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' Interact: state=%s, interactor=%s"),
        *GetName(),
        *UEnum::GetValueAsString(CurrentState),
        *GetNameSafe(Interactor));

    LastInteractor = Interactor;

    switch (CurrentState)
    {
    case EDoorState::Locked:
    {
        UInventorySubsystem* Inventory = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UInventorySubsystem>()
            : nullptr;

        UItemDefinition* KeyDef  = RequiredKey.LoadSynchronous();
        const bool       bHasKey = (Inventory != nullptr) && (KeyDef != nullptr) && Inventory->HasItem(KeyDef);

        if (bHasKey)
        {
            HandleUnlockAndOpen(Interactor);
        }
        else
        {
            HandleLockedAttempt();
        }
        break;
    }
    case EDoorState::Closed:
    {
        TransitionToOpen();
        break;
    }
    case EDoorState::Open:
    {
        // Defensive — InteractionSphere collision is disabled while Open, so
        // the trace should not reach here. If something else routes Interact
        // here (e.g. a debug shortcut), ignore silently.
        UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' Interact while Open — ignored."), *GetName());
        break;
    }
    }
}

// ===========================================================================
// State transitions
// ===========================================================================

void ADoor::HandleUnlockAndOpen(AActor* Interactor)
{
    UInventorySubsystem* Inventory = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UInventorySubsystem>()
        : nullptr;
    UItemDefinition* KeyDef = RequiredKey.LoadSynchronous();

    if (!Inventory || !KeyDef)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Door] '%s' HandleUnlockAndOpen: missing Inventory (%s) or KeyDef (%s)."),
            *GetName(),
            Inventory ? TEXT("ok") : TEXT("null"),
            KeyDef    ? TEXT("ok") : TEXT("null"));
        return;
    }

    const EInventoryResult RemoveResult = Inventory->RemoveItem(KeyDef, 1);
    if (RemoveResult != EInventoryResult::Success)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Door] '%s' HandleUnlockAndOpen: RemoveItem on '%s' failed with code %d — aborting unlock."),
            *GetName(), *KeyDef->DisplayName.ToString(), (int32)RemoveResult);
        return;
    }

    bIsUnlocked = true;

    UE_LOG(LogTemp, Log,
        TEXT("[Door] '%s' UNLOCKED by '%s' — consumed key '%s'."),
        *GetName(),
        *GetNameSafe(Interactor),
        *KeyDef->DisplayName.ToString());

    EmitDoorNoise(UnlockNoise, TEXT("Unlock"));
    BP_OnUnlocked();

    TransitionToOpen();
}

void ADoor::HandleLockedAttempt()
{
    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' LOCKED ATTEMPT — player has no valid key."), *GetName());

    EmitDoorNoise(LockedAttemptNoise, TEXT("LockedAttempt"));

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    AExplorationPlayerController* ExpPC = Cast<AExplorationPlayerController>(PC);
    if (ExpPC)
    {
        const FText Message = LockedPromptText.IsEmpty()
            ? NSLOCTEXT("Door", "FallbackLockedPrompt", "It's locked.")
            : LockedPromptText;
        ExpPC->ShowExplorationPrompt(Message, 1.f);
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Door] '%s' HandleLockedAttempt: AExplorationPlayerController unavailable — prompt skipped."),
            *GetName());
    }

    BP_OnLockedAttempt();
}

void ADoor::TransitionToOpen()
{
    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' Closed -> Open."), *GetName());

    CurrentState = EDoorState::Open;
    ApplyCollisionForState(CurrentState);

    EmitDoorNoise(OpenNoise, TEXT("Open"));
    BP_OnDoorOpening();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            AutoCloseTimerHandle,
            this,
            &ADoor::TransitionToClosed,
            AutoCloseDelaySeconds,
            /*bLoop=*/false);

        UE_LOG(LogTemp, Verbose,
            TEXT("[Door] '%s' auto-close timer armed (%.2fs)."),
            *GetName(), AutoCloseDelaySeconds);
    }
}

void ADoor::TransitionToClosed()
{
    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' Open -> Closed (auto-close)."), *GetName());

    CurrentState = EDoorState::Closed;
    ApplyCollisionForState(CurrentState);

    EmitDoorNoise(CloseNoise, TEXT("Close"));
    BP_OnDoorClosing();
}

void ADoor::ApplyCollisionForState(EDoorState State)
{
    if (!DoorCollisionBox || !InteractionSphere)
    {
        return;
    }

    switch (State)
    {
    case EDoorState::Locked:
    case EDoorState::Closed:
        // Solid box + active interaction trigger.
        DoorCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        break;

    case EDoorState::Open:
        // Door is out of the way visually (BP plays the swing); fully disable
        // collision so the player walks through and the interact trace ignores
        // the door entirely until it auto-closes.
        DoorCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        break;
    }
}

// ===========================================================================
// Noise
// ===========================================================================

void ADoor::EmitDoorNoise(const UNoiseDefinition* Noise, const TCHAR* DebugLabel) const
{
    if (!Noise)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("[Door] '%s' EmitDoorNoise(%s): no noise assigned — silent."),
            *GetName(), DebugLabel);
        return;
    }

    UExcursionNoiseSubsystem* NoiseSS = UExcursionNoiseSubsystem::Get(this);
    if (!NoiseSS)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Door] '%s' EmitDoorNoise(%s): UExcursionNoiseSubsystem unavailable."),
            *GetName(), DebugLabel);
        return;
    }

    // Centre of the leaf rather than the floor pivot. The occlusion trace
    // ignores the door itself here, because it touches the noise source.
    const FVector NoiseLocation = DoorCollisionBox
        ? DoorCollisionBox->GetComponentLocation()
        : GetActorLocation();

    NoiseSS->EmitNoise(Noise, NoiseLocation, LastInteractor.Get());
}

// ===========================================================================
// ISaveParticipant
// ===========================================================================

void ADoor::WriteSaveData_Implementation(FActorSaveData& OutData) const
{
    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes, true);
    FObjectAndNameAsStringProxyArchive Proxy(Writer, false);

    bool TempUnlocked = bIsUnlocked;
    Proxy << TempUnlocked;

    FSavePayload Payload;
    Payload.Data = MoveTemp(Bytes);
    OutData.CustomDataMap.Add(SaveKey, MoveTemp(Payload));

    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' WriteSaveData: bIsUnlocked=%s"),
        *GetName(),
        bIsUnlocked ? TEXT("true") : TEXT("false"));
}

void ADoor::ReadSaveData_Implementation(const FActorSaveData& InData)
{
    const FSavePayload* Payload = InData.CustomDataMap.Find(SaveKey);
    if (!Payload)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Door] '%s' ReadSaveData: no payload for key '%s' — keeping defaults (bIsUnlocked stays false)."),
            *GetName(), *SaveKey.ToString());
        return;
    }

    FMemoryReader Reader(Payload->Data, true);
    FObjectAndNameAsStringProxyArchive Proxy(Reader, false);

    Proxy << bIsUnlocked;

    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' ReadSaveData: bIsUnlocked=%s"),
        *GetName(),
        bIsUnlocked ? TEXT("true") : TEXT("false"));
}

void ADoor::OnPostRestore_Implementation()
{
    // Two-path initialization, runs once per level load BEFORE BeginPlay:
    //   - First visit: bIsUnlocked stays false (no payload was read) → state
    //     comes from the bStartLocked designer flag.
    //   - Revisit:     bIsUnlocked was set by ReadSaveData → if true, the
    //     door starts Closed (Open is never persisted).
    CurrentState = bIsUnlocked
        ? EDoorState::Closed
        : (bStartLocked ? EDoorState::Locked : EDoorState::Closed);

    ApplyCollisionForState(CurrentState);

    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' OnPostRestore: bIsUnlocked=%d → state=%s"),
        *GetName(),
        (int32)bIsUnlocked,
        *UEnum::GetValueAsString(CurrentState));
}