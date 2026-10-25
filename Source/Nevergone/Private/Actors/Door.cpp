// Copyright Xyzto Works

#include "Actors/Door.h"

#include "ActorComponents/CombatPresenceComponent.h"
#include "ActorComponents/InteractableComponent.h"
#include "ActorComponents/SaveableComponent.h"
#include "Characters/PlayerControllers/ExplorationPlayerController.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Data/ActorSaveData.h"
#include "Data/ItemDefinition.h"
#include "Data/NoiseDefinition.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineTools/CollisionChannels.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameInstance/SaveKeys.h"
#include "Interfaces/DoorAgent.h"
#include "Kismet/GameplayStatics.h"
#include "NavLinkCustomComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "TimerManager.h"
#include "Widgets/ExplorationPromptWidget.h"
#include "World/ExcursionNoiseSubsystem.h"
#include "World/InventorySubsystem.h"

const FName ADoor::SaveKey = SaveKeys::Door;

namespace DoorConstants
{
    // An agent walking the link keeps the doorway occupied while it is
    // within the link's length of the leaf plus this much, and no further
    // than the leaf's half width plus this much to the side.
    static constexpr float CrossingClearance = 20.f;

    // A zero delay still goes through the timer, so an agent is never
    // released from inside the call that paused it.
    static constexpr float MinAgentOpenDelaySeconds = 0.01f;

    // How far below the box centre each nav link end looks for the floor.
    static constexpr float NavLinkFloorSearchDepth = 500.f;

    // A zero duration swings the leaf in one frame.
    static constexpr float MinSwingDurationSeconds = 0.01f;
}

// ===========================================================================
// Construction
// ===========================================================================

ADoor::ADoor()
{
    // Ticks only while the leaf swings.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    // Plain SceneComponent root keeps the actor pivot independent from the
    // mesh pivot, so the door leaf can swing around its hinge without
    // forcing the whole actor to rotate.
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // The leaf, swung by C++ around its pivot (the hinge). Queries only:
    // it takes the interact trace (the player closes an open door through
    // the leaf) and blocks sight. It never blocks pawns, the camera or
    // hearing: a swinging leaf with physics shoves whoever is in its arc,
    // and the static navmesh cannot route agents around an open one. Closed,
    // the box does all the blocking for the doorway. Re-applied after the
    // Blueprint and the level apply their saved values (ApplyLeafCollision).
    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
    DoorMesh->SetupAttachment(Root);
    DoorMesh->SetCanEverAffectNavigation(false);
    ApplyLeafCollision();

    // Actual blocking volume. It must cover the whole doorway: a gap at a
    // jamb lets a pawn squeeze past and lets sight through. The BP subclass
    // may still tune BoxExtent and RelativeLocation per door asset (X is the
    // thickness, Y half the width, Z half the height, lifted by Z so the
    // bottom sits at the actor pivot).
    //
    // Default sized for the minimum clear opening of 110:
    //   Extents = (5, 60, 100)  →  10x120x200 cm full size
    DoorCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorCollisionBox"));
    DoorCollisionBox->SetupAttachment(Root);
    DoorCollisionBox->SetBoxExtent(FVector(5.f, 60.f, 100.f));
    DoorCollisionBox->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
    DoorCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    DoorCollisionBox->SetCollisionObjectType(ECC_WorldStatic);
    DoorCollisionBox->SetCollisionResponseToAllChannels(ECR_Block);
    // The Interactable channel will be promoted to Block by
    // InteractableComponent::OnRegister regardless — that is fine. The
    // sphere's only purpose then is widening the interaction radius beyond
    // the visual door footprint. A trace starting inside the sphere misses
    // it, so up close the open door is reached through the leaf.

    // Interaction trigger volume — wider than the box so the player can
    // interact from a small distance, similar to AContainerActor / AWorldItemActor.
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(Root);
    InteractionSphere->SetSphereRadius(110.f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);

    // Smart link across the doorway. The navmesh is static and the box
    // carves the doorway out of it, open or closed; the link is the only
    // way a path crosses. Ends are placed in OnConstruction.
    DoorNavLink = CreateDefaultSubobject<UNavLinkCustomComponent>(TEXT("DoorNavLink"));

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

void ADoor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    ApplyLeafCollision();
    CaptureClosedLeaf();
    PlaceNavLinkEnds();
}

void ADoor::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    // Construction scripts do not run again on a level loaded in a cooked
    // game: repeat what the leaf needs here.
    ApplyLeafCollision();
    CaptureClosedLeaf();
}

void ADoor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const float Speed = OpenAngleDegrees / FMath::Max(SwingDurationSeconds, DoorConstants::MinSwingDurationSeconds);

    SwingYaw = FMath::FInterpConstantTo(SwingYaw, TargetSwingYaw, DeltaSeconds, Speed);

    if (bJolting)
    {
        JoltElapsed += DeltaSeconds;

        bJolting = JoltElapsed < JoltDurationSeconds;
    }

    ApplyLeafRotation();

    if (!bJolting && FMath::IsNearlyEqual(SwingYaw, TargetSwingYaw))
    {
        SetActorTickEnabled(false);
    }
}

FVector ADoor::GetDoorwayAxis() const
{
    // The box's thickness axis, in actor space and flat: the direction the
    // nav link crosses and the leaf swings towards.
    const FVector Axis = DoorCollisionBox
        ? DoorCollisionBox->GetRelativeRotation().RotateVector(FVector::ForwardVector).GetSafeNormal2D()
        : FVector::ZeroVector;

    return Axis.IsNearlyZero() ? FVector::ForwardVector : Axis;
}

float ADoor::GetSideOf(const AActor* Actor) const
{
    if (!Actor || !DoorCollisionBox)
    {
        return 0.f;
    }

    // In actor space, from the middle of the box along its thickness axis.
    const FVector ActorLocal = GetActorTransform().InverseTransformPosition(Actor->GetActorLocation());

    return FVector::DotProduct(ActorLocal - DoorCollisionBox->GetRelativeLocation(), GetDoorwayAxis());
}

bool ADoor::IsOnLatchSide(const AActor* Actor) const
{
    return Actor && DoorRules::IsOnLatchSide(LatchSide, GetSideOf(Actor));
}

void ADoor::PlaceNavLinkEnds()
{
    if (!DoorNavLink || !DoorCollisionBox)
    {
        return;
    }

    // Link ends in actor space, one on each side of the box along its
    // thickness axis. Runs in the editor, so the navmesh built there already
    // has the link where the door is.
    const FVector BoxLocation = DoorCollisionBox->GetRelativeLocation();
    const FVector Axis = GetDoorwayAxis();

    FVector Ends[2] =
    {
        FVector(BoxLocation.X, BoxLocation.Y, 0.f) + Axis * NavLinkHalfLength,
        FVector(BoxLocation.X, BoxLocation.Y, 0.f) - Axis * NavLinkHalfLength,
    };

    // Each end must sit within a step height of the navmesh or it does not
    // snap, and the link silently never exists. The actor pivot is not
    // reliably on the floor, so each end looks for the floor below it,
    // starting at the top of the box.
    if (const UWorld* World = GetWorld())
    {
        const FTransform& ActorTransform = GetActorTransform();

        const float TraceTopZ = BoxLocation.Z + DoorCollisionBox->GetUnscaledBoxExtent().Z;

        FCollisionQueryParams Params(SCENE_QUERY_STAT(DoorNavLinkFloor), false, this);

        for (FVector& End : Ends)
        {
            const FVector Start = ActorTransform.TransformPosition(FVector(End.X, End.Y, TraceTopZ));
            const FVector Bottom = Start - FVector::UpVector * DoorConstants::NavLinkFloorSearchDepth;

            FHitResult Hit;

            if (World->LineTraceSingleByChannel(Hit, Start, Bottom, ECC_Visibility, Params))
            {
                End.Z = ActorTransform.InverseTransformPosition(Hit.ImpactPoint).Z;
            }
            else
            {
                // Left at the pivot: the link only works if the pivot is near
                // the navmesh.
                UE_LOG(LogTemp, Warning,
                    TEXT("[Door] '%s' found no floor under a nav link end at %s — AI may not cross this door."),
                    *GetName(), *Start.ToCompactString());
            }
        }
    }

    DoorNavLink->SetLinkData(Ends[0], Ends[1], ENavLinkDirection::BothWays);
}

// ===========================================================================
// Leaf swing
// ===========================================================================

void ADoor::ApplyLeafCollision()
{
    if (!DoorMesh)
    {
        return;
    }

    // Collision is part of the door's behaviour, not of the art: a value
    // saved on the Blueprint or the placed instance is overridden. Logged
    // only after construction, where the engine defaults always differ.
    if (!HasAnyFlags(RF_NeedInitialization) &&
        (DoorMesh->GetCollisionEnabled() != ECollisionEnabled::QueryOnly ||
         DoorMesh->GetCollisionResponseToChannel(NevergoneCollision::Interactable) != ECR_Block))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' DoorMesh collision reset to the leaf setup (QueryOnly)."),
            *GetName());
    }

    DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DoorMesh->SetCollisionObjectType(ECC_WorldDynamic);
    DoorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    DoorMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    DoorMesh->SetCollisionResponseToChannel(NevergoneCollision::Interactable, ECR_Block);
}

void ADoor::CaptureClosedLeaf()
{
    if (DoorMesh)
    {
        ClosedLeafRotation = DoorMesh->GetRelativeRotation().Quaternion();
    }

    SwingYaw = 0.f;
    TargetSwingYaw = 0.f;
    bJolting = false;
}

float ADoor::ChooseSwingSign(const AActor* Opener) const
{
    const UStaticMesh* Mesh = DoorMesh ? DoorMesh->GetStaticMesh() : nullptr;

    if (!Opener || !Mesh || !DoorCollisionBox)
    {
        return 1.f;
    }

    const FVector Axis = GetDoorwayAxis();

    // Side of the doorway the opener is on, in actor space.
    const float OpenerSide = GetSideOf(Opener);

    // Middle of the closed leaf relative to the hinge (the mesh pivot), in
    // actor space. Works whichever side the hinge is on.
    const FVector LeafCentre = ClosedLeafRotation.RotateVector(Mesh->GetBounds().Origin * DoorMesh->GetRelativeScale3D());

    return DoorRules::ChooseSwingSign(OpenerSide, LeafCentre, OpenAngleDegrees, Axis);
}

void ADoor::StartSwing(float NewTargetYaw)
{
    TargetSwingYaw = NewTargetYaw;

    SetActorTickEnabled(bJolting || !FMath::IsNearlyEqual(SwingYaw, TargetSwingYaw));
}

void ADoor::StartJolt(float Sign, AActor* JoltInstigator)
{
    UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' jolts (%s, by '%s'): %.1f deg over %.2fs."),
        *GetName(), *UEnum::GetValueAsString(CurrentState), *GetNameSafe(JoltInstigator),
        JoltAngleDegrees, JoltDurationSeconds);

    // Movement only: the leaf has no collision to push anybody with.
    JoltSign = Sign >= 0.f ? 1.f : -1.f;
    JoltElapsed = 0.f;
    bJolting = JoltDurationSeconds > 0.f && JoltAngleDegrees > 0.f;

    if (bJolting)
    {
        SetActorTickEnabled(true);
    }

    EmitDoorNoise(JoltNoise, TEXT("Jolt"), JoltInstigator);
}

void ADoor::ApplyLeafRotation()
{
    if (DoorMesh)
    {
        const float JoltYaw = bJolting
            ? JoltSign * DoorRules::GetJoltOffset(JoltElapsed, JoltDurationSeconds, JoltAngleDegrees)
            : 0.f;

        // Yaw about the actor's up axis, on top of the authored closed pose,
        // around the mesh pivot (the hinge).
        DoorMesh->SetRelativeRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(SwingYaw + JoltYaw)) * ClosedLeafRotation);
    }
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

    if (DoorNavLink)
    {
        DoorNavLink->SetMoveReachedLink(this, &ADoor::HandleAgentReachedLink);

        // The navmesh registers only after OnPostRestore, so the link state
        // set there never reached it (and SetEnabled skips an unchanged
        // value). Push the current state to the loaded navmesh now.
        if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
        {
            NavSys->UpdateCustomLink(DoorNavLink);
        }
    }

    if (bStartLocked && !bIsUnlocked && RequiredKey.IsNull())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Door] '%s' starts Locked but has no RequiredKey assigned — it can never be unlocked."),
            *GetName());
    }

    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' BeginPlay: state=%s bIsUnlocked=%d"),
        *GetName(),
        *UEnum::GetValueAsString(CurrentState),
        (int32)bIsUnlocked);
}

void ADoor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Nobody stays paused at a door that no longer exists.
    ReleaseWaitingAgents();

    Super::EndPlay(EndPlayReason);
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

    ApplyInteraction(DoorRules::ResolveInteraction(CurrentState, IsOnLatchSide(Interactor), /*bHold=*/false), Interactor);
}

bool ADoor::CanInteractHold(const AActor* Interactor) const
{
    return DoorRules::CanHoldInteract(CurrentState, IsOnLatchSide(Interactor));
}

void ADoor::InteractHold(AActor* Interactor)
{
    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' InteractHold: state=%s, interactor=%s"),
        *GetName(),
        *UEnum::GetValueAsString(CurrentState),
        *GetNameSafe(Interactor));

    ApplyInteraction(DoorRules::ResolveInteraction(CurrentState, IsOnLatchSide(Interactor), /*bHold=*/true), Interactor);
}

void ADoor::ApplyInteraction(EDoorInteraction Interaction, AActor* Interactor)
{
    switch (Interaction)
    {
    case EDoorInteraction::UseKey:
        HandleKeyInteraction(Interactor);
        break;

    case EDoorInteraction::Open:
        TryOpen(Interactor);
        break;

    case EDoorInteraction::Close:
        TryClose(Interactor);
        break;

    case EDoorInteraction::CloseAndLatch:
        // Somebody in the doorway: the leaf jolts and the latch stays off.
        if (!TryClose(Interactor))
        {
            break;
        }
        [[fallthrough]];

    case EDoorInteraction::Latch:
        // Not an AI noise.
        UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' latched by '%s'."), *GetName(), *GetNameSafe(Interactor));
        SetState(EDoorState::Latched);
        break;

    case EDoorInteraction::Unlatch:
        UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' unlatched by '%s'."), *GetName(), *GetNameSafe(Interactor));
        SetState(EDoorState::Closed);
        break;

    case EDoorInteraction::UnlatchAndOpen:
        UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' unlatched by '%s'."), *GetName(), *GetNameSafe(Interactor));
        SetState(EDoorState::Closed);
        TryOpen(Interactor);
        break;

    case EDoorInteraction::Jolt:
        // The latch holds: the leaf gives a little away from the one pushing.
        StartJolt(ChooseSwingSign(Interactor), Interactor);
        break;

    case EDoorInteraction::None:
    default:
        UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' interact by '%s' does nothing: %s."),
            *GetName(), *GetNameSafe(Interactor), *UEnum::GetValueAsString(CurrentState));
        break;
    }
}

void ADoor::HandleKeyInteraction(AActor* Interactor)
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
        HandleLockedAttempt(Interactor);
    }
}

// ===========================================================================
// Open / close
// ===========================================================================

bool ADoor::TryOpen(AActor* Opener)
{
    switch (CurrentState)
    {
    case EDoorState::Locked:
    case EDoorState::Latched:
        UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' TryOpen by '%s' refused: %s."),
            *GetName(), *GetNameSafe(Opener), *UEnum::GetValueAsString(CurrentState));
        return false;

    case EDoorState::Open:
    case EDoorState::Broken:
        ReleaseWaitingAgents();
        return true;

    case EDoorState::Closed:
    default:
        break;
    }

    UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' Closed -> Open by '%s'."), *GetName(), *GetNameSafe(Opener));

    SetState(EDoorState::Open);
    StartSwing(ChooseSwingSign(Opener) * OpenAngleDegrees);

    EmitDoorNoise(OpenNoise, TEXT("Open"), Opener);
    BP_OnDoorOpening();

    // Whoever opened it, anyone waiting at the link walks through now.
    ReleaseWaitingAgents();

    return true;
}

bool ADoor::TryClose(AActor* Closer)
{
    if (CurrentState != EDoorState::Open)
    {
        return CurrentState != EDoorState::Broken;
    }

    if (IsDoorwayOccupied())
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' TryClose by '%s' refused: somebody is in the doorway."),
            *GetName(), *GetNameSafe(Closer));

        // The leaf starts to close, hits whoever is there and swings back.
        StartJolt(TargetSwingYaw > 0.f ? -1.f : 1.f, Closer);

        return false;
    }

    UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' Open -> Closed by '%s'."), *GetName(), *GetNameSafe(Closer));

    SetState(EDoorState::Closed);
    StartSwing(0.f);

    EmitDoorNoise(CloseNoise, TEXT("Close"), Closer);
    BP_OnDoorClosing();

    return true;
}

bool ADoor::IsDoorwayOccupied() const
{
    const UWorld* World = GetWorld();

    if (!World || !DoorCollisionBox)
    {
        return false;
    }

    // A pawn whose capsule touches the box would be shut inside the doorway.
    TArray<FOverlapResult> Overlaps;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(DoorwayOccupancy), false, this);

    World->OverlapMultiByObjectType(
        Overlaps,
        DoorCollisionBox->GetComponentLocation(),
        DoorCollisionBox->GetComponentQuat(),
        FCollisionObjectQueryParams(ECC_Pawn),
        FCollisionShape::MakeBox(DoorCollisionBox->GetScaledBoxExtent()),
        Params);

    for (const FOverlapResult& Overlap : Overlaps)
    {
        // Only the capsule counts: character meshes are Pawn objects too and
        // reach further, so the player closing the door would block himself.
        const APawn* Pawn = Cast<APawn>(Overlap.GetActor());

        if (Pawn && Overlap.GetComponent() == Pawn->GetRootComponent())
        {
            return true;
        }
    }

    // An agent already walking the link is in the doorway before it reaches
    // the leaf: closing now would shut the door in its face.
    for (const FAgentMove& Agent : CrossingAgents)
    {
        if (IsStillCrossing(Agent))
        {
            return true;
        }
    }

    return false;
}

bool ADoor::TryBreak(AActor* Breaker)
{
    if (CurrentState != EDoorState::Latched)
    {
        return false;
    }

    UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' Latched -> Broken by '%s'."), *GetName(), *GetNameSafe(Breaker));

    SetState(EDoorState::Broken);

    // Thrown open away from the one who broke it, for good.
    StartSwing(ChooseSwingSign(Breaker) * OpenAngleDegrees);

    BP_OnDoorBroken();

    ReleaseWaitingAgents();

    return true;
}

bool ADoor::IsAgentWaiting(const UPathFollowingComponent* PathFollowing) const
{
    return PathFollowing && WaitingAgents.ContainsByPredicate([PathFollowing](const FAgentMove& Agent)
    {
        return Agent.PathFollowing.Get() == PathFollowing && IsStillWaiting(Agent);
    });
}

FVector ADoor::GetNoiseLocation() const
{
    // Centre of the box (the closed doorway) rather than the floor pivot.
    return DoorCollisionBox ? DoorCollisionBox->GetComponentLocation() : GetActorLocation();
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

    // Closed first: from here on the door is an ordinary door, for AI too.
    SetState(EDoorState::Closed);

    EmitDoorNoise(UnlockNoise, TEXT("Unlock"), Interactor);
    BP_OnUnlocked();

    TryOpen(Interactor);
}

void ADoor::HandleLockedAttempt(AActor* Interactor)
{
    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' LOCKED ATTEMPT — player has no valid key."), *GetName());

    EmitDoorNoise(LockedAttemptNoise, TEXT("LockedAttempt"), Interactor);

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

void ADoor::SetState(EDoorState NewState)
{
    CurrentState = NewState;
    ApplyState();
    UpdateWaitingAgents();
}

void ADoor::ApplyState()
{
    if (DoorCollisionBox && InteractionSphere)
    {
        switch (CurrentState)
        {
        case EDoorState::Locked:
        case EDoorState::Closed:
        case EDoorState::Latched:
            // Solid box: blocks pawns, sight and hearing occlusion.
            DoorCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            break;

        case EDoorState::Open:
            // The leaf swings out of the way (StartSwing) and still takes
            // the interact trace; the box leaves the doorway free. The
            // sphere stays as a wider interact target.
            DoorCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            break;

        case EDoorState::Broken:
            // Nothing left to interact with: it never closes again. The leaf
            // still blocks sight.
            DoorCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

            if (DoorMesh)
            {
                DoorMesh->SetCollisionResponseToChannel(NevergoneCollision::Interactable, ECR_Ignore);
            }
            break;
        }
    }

    // Updates the link's area in the loaded navmesh: no rebuild, so it
    // works on a static navmesh.
    if (DoorNavLink)
    {
        DoorNavLink->SetEnabled(DoorRules::IsAgentLinkEnabled(CurrentState));
    }
}

// ===========================================================================
// AI passage
// ===========================================================================

void ADoor::HandleAgentReachedLink(UNavLinkCustomComponent* Link, UObject* PathComp, const FVector& DestPoint)
{
    UPathFollowingComponent* PathFollowing = Cast<UPathFollowingComponent>(PathComp);

    if (!PathFollowing)
    {
        return;
    }

    const EDoorAgentPassage Passage = DoorRules::ResolveAgentPassage(CurrentState);

    UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' agent '%s' reached the link: state=%s, passage=%s."),
        *GetName(),
        *GetNameSafe(GetAgentPawn(PathFollowing)),
        *UEnum::GetValueAsString(CurrentState),
        *UEnum::GetValueAsString(Passage));

    switch (Passage)
    {
    case EDoorAgentPassage::Pass:
        PathFollowing->FinishUsingCustomLink(Link);
        AddCrossingAgent({ PathFollowing, PathFollowing->GetCurrentRequestId() });
        break;

    case EDoorAgentPassage::OpenFirst:
    case EDoorAgentPassage::Barred:
    {
        const FAIRequestID RequestId = PathFollowing->GetCurrentRequestId();

        PathFollowing->PauseMove(RequestId, EPathFollowingVelocityMode::Reset);

        // An agent that re-requested its move while waiting (a chase
        // repath) comes back here with a new request: keep one entry.
        FAgentMove* Existing = WaitingAgents.FindByPredicate([PathFollowing](const FAgentMove& Agent)
        {
            return Agent.PathFollowing.Get() == PathFollowing;
        });

        if (Existing)
        {
            Existing->RequestId = RequestId;
        }
        else
        {
            WaitingAgents.Add({ PathFollowing, RequestId });
        }

        if (Passage == EDoorAgentPassage::OpenFirst)
        {
            StartAgentOpenTimer();
        }
        else
        {
            NotifyAgentBarred(PathFollowing);
        }
        break;
    }

    case EDoorAgentPassage::Blocked:
        // The link is off while the door is a wall, so no path should get
        // here. If one did (a path built before the state changed), stop it
        // instead of pushing into the box.
        PathFollowing->AbortMove(*this, FPathFollowingResultFlags::InvalidPath);
        break;
    }
}

void ADoor::HandleAgentOpenDelayElapsed()
{
    WaitingAgents.RemoveAll([](const FAgentMove& Agent)
    {
        return !IsStillWaiting(Agent);
    });

    if (WaitingAgents.Num() == 0)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' open delay elapsed with nobody waiting — stays %s."),
            *GetName(), *UEnum::GetValueAsString(CurrentState));
        return;
    }

    // The first agent still waiting is the one who opens it: the noise is
    // theirs. TryOpen releases everybody.
    AActor* Opener = GetAgentPawn(WaitingAgents[0].PathFollowing.Get());

    if (!TryOpen(Opener))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' could not be opened by '%s' — aborting %d waiting move(s)."),
            *GetName(), *GetNameSafe(Opener), WaitingAgents.Num());

        TArray<FAgentMove> Aborted = MoveTemp(WaitingAgents);
        WaitingAgents.Reset();

        for (const FAgentMove& Agent : Aborted)
        {
            if (UPathFollowingComponent* PathFollowing = Agent.PathFollowing.Get())
            {
                PathFollowing->AbortMove(*this, FPathFollowingResultFlags::InvalidPath, Agent.RequestId);
            }
        }
    }
}

void ADoor::ReleaseWaitingAgents()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AgentOpenTimerHandle);
    }

    if (WaitingAgents.Num() == 0)
    {
        return;
    }

    TArray<FAgentMove> Released = MoveTemp(WaitingAgents);
    WaitingAgents.Reset();

    for (const FAgentMove& Agent : Released)
    {
        if (!IsStillWaiting(Agent))
        {
            continue;
        }

        UPathFollowingComponent* PathFollowing = Agent.PathFollowing.Get();

        UE_LOG(LogTemp, Verbose, TEXT("[Door] '%s' releases agent '%s'."),
            *GetName(), *GetNameSafe(GetAgentPawn(PathFollowing)));

        PathFollowing->FinishUsingCustomLink(DoorNavLink);
        PathFollowing->ResumeMove(Agent.RequestId);

        AddCrossingAgent(Agent);
    }
}

void ADoor::UpdateWaitingAgents()
{
    WaitingAgents.RemoveAll([](const FAgentMove& Agent)
    {
        return !IsStillWaiting(Agent);
    });

    switch (DoorRules::ResolveAgentPassage(CurrentState))
    {
    case EDoorAgentPassage::OpenFirst:
        // Unlatched with agents still at the door: they open it after the
        // usual delay.
        if (WaitingAgents.Num() > 0)
        {
            StartAgentOpenTimer();
        }
        break;

    case EDoorAgentPassage::Barred:
    {
        // Latched while agents waited to open it: it will not open for them now.
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(AgentOpenTimerHandle);
        }

        const TArray<FAgentMove> Barred = WaitingAgents;

        for (const FAgentMove& Agent : Barred)
        {
            NotifyAgentBarred(Agent.PathFollowing.Get());
        }
        break;
    }

    default:
        // Open and Broken release them where they open (TryOpen, TryBreak).
        break;
    }
}

void ADoor::StartAgentOpenTimer()
{
    // The delay counts from the first agent to arrive.
    if (UWorld* World = GetWorld(); World && !World->GetTimerManager().IsTimerActive(AgentOpenTimerHandle))
    {
        World->GetTimerManager().SetTimer(
            AgentOpenTimerHandle,
            this,
            &ADoor::HandleAgentOpenDelayElapsed,
            FMath::Max(AgentOpenDelaySeconds, DoorConstants::MinAgentOpenDelaySeconds),
            /*bLoop=*/false);
    }
}

void ADoor::NotifyAgentBarred(UPathFollowingComponent* PathFollowing)
{
    if (IDoorAgent* Agent = PathFollowing ? Cast<IDoorAgent>(PathFollowing->GetOwner()) : nullptr)
    {
        Agent->HandleDoorBarred(this);
    }
}

void ADoor::AddCrossingAgent(const FAgentMove& Agent)
{
    CrossingAgents.RemoveAll([this, &Agent](const FAgentMove& Other)
    {
        return Other.PathFollowing == Agent.PathFollowing || !IsStillCrossing(Other);
    });

    CrossingAgents.Add(Agent);
}

bool ADoor::IsStillCrossing(const FAgentMove& Agent) const
{
    const UPathFollowingComponent* PathFollowing = Agent.PathFollowing.Get();

    if (!PathFollowing ||
        PathFollowing->GetStatus() == EPathFollowingStatus::Idle ||
        PathFollowing->GetCurrentRequestId().GetID() != Agent.RequestId.GetID())
    {
        return false;
    }

    const AActor* Pawn = GetAgentPawn(PathFollowing);

    if (!Pawn || !DoorCollisionBox)
    {
        return false;
    }

    // In the box's frame: X across the doorway (the link), Y along the leaf.
    const FVector Local = DoorCollisionBox->GetComponentTransform().InverseTransformPositionNoScale(Pawn->GetActorLocation());

    return FMath::Abs(Local.X) <= NavLinkHalfLength + DoorConstants::CrossingClearance &&
        FMath::Abs(Local.Y) <= DoorCollisionBox->GetScaledBoxExtent().Y + DoorConstants::CrossingClearance;
}

bool ADoor::IsStillWaiting(const FAgentMove& Agent)
{
    const UPathFollowingComponent* PathFollowing = Agent.PathFollowing.Get();

    return PathFollowing &&
        PathFollowing->GetStatus() == EPathFollowingStatus::Paused &&
        PathFollowing->GetCurrentRequestId().GetID() == Agent.RequestId.GetID();
}

AActor* ADoor::GetAgentPawn(const UPathFollowingComponent* PathFollowing)
{
    // The path following component lives on the AI controller.
    const AController* Controller = PathFollowing ? Cast<AController>(PathFollowing->GetOwner()) : nullptr;

    return Controller ? Controller->GetPawn() : nullptr;
}

// ===========================================================================
// Noise
// ===========================================================================

void ADoor::EmitDoorNoise(const UNoiseDefinition* Noise, const TCHAR* DebugLabel, AActor* NoiseInstigator) const
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

    // The door is the noise's source: hearing occlusion ignores it, so the
    // closed box never muffles its own noise, whatever the listener's angle.
    NoiseSS->EmitNoise(Noise, GetNoiseLocation(), NoiseInstigator, nullptr, this);
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
    //     door starts Closed (Open, Latched and Broken are never persisted).
    // The navmesh is not registered yet: BeginPlay pushes the nav link
    // state to it.
    SetState(bIsUnlocked
        ? EDoorState::Closed
        : (bStartLocked ? EDoorState::Locked : EDoorState::Closed));

    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' OnPostRestore: bIsUnlocked=%d → state=%s"),
        *GetName(),
        (int32)bIsUnlocked,
        *UEnum::GetValueAsString(CurrentState));
}
