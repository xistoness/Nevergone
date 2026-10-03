// Copyright Xyzto Works

#include "ActorComponents/CombatPresenceComponent.h"

#include "World/CombatActorScopeSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

UCombatPresenceComponent::UCombatPresenceComponent()
{
    // Tag-only component — no per-frame work.
    PrimaryComponentTick.bCanEverTick = false;
}

void UCombatPresenceComponent::BeginPlay()
{
    Super::BeginPlay();

    if (UCombatActorScopeSubsystem* Sub = GetScopeSubsystem())
    {
        Sub->RegisterPresenceComponent(this);
    }
    else
    {
        UE_LOG(LogTemp, Error,
            TEXT("[CombatPresenceComponent] BeginPlay: UCombatActorScopeSubsystem not found in world. Owner='%s'"),
            *GetNameSafe(GetOwner()));
    }
}

void UCombatPresenceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UCombatActorScopeSubsystem* Sub = GetScopeSubsystem())
    {
        Sub->UnregisterPresenceComponent(this);
    }

    Super::EndPlay(EndPlayReason);
}

void UCombatPresenceComponent::CaptureSnapshot()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        bSnapshotValid = false;
        return;
    }

    // Idempotency guard.
    //
    // If a snapshot is already valid, a previous EnterCombatPresenceMode has
    // already recorded the original pre-combat state for this actor. A second
    // CaptureSnapshot call would read the post-policy state (e.g. Hidden=true
    // for HideAndPause actors) and store that as the "natural" state, so the
    // matching RestoreFromSnapshot would never restore the actor.
    //
    // This can happen if EnterCombatPresenceMode is called twice without a
    // matching ExitCombatPresenceMode in between — for example during a
    // recursive state transition triggered by an unexpected re-entry into
    // BattlePreparation. The original snapshot is the correct one to keep.
    if (bSnapshotValid)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[CombatPresenceComponent] CaptureSnapshot on '%s' skipped — snapshot already valid (preserving original pre-combat state)."),
            *Owner->GetName());
        return;
    }

    bSnapshot_Hidden           = Owner->IsHidden();
    bSnapshot_CollisionEnabled = Owner->GetActorEnableCollision();
    bSnapshot_TickEnabled      = Owner->IsActorTickEnabled();
    bSnapshotValid             = true;

    UE_LOG(LogTemp, Verbose,
        TEXT("[CombatPresenceComponent] CaptureSnapshot on '%s': hidden=%d collision=%d tick=%d"),
        *Owner->GetName(),
        bSnapshot_Hidden ? 1 : 0,
        bSnapshot_CollisionEnabled ? 1 : 0,
        bSnapshot_TickEnabled ? 1 : 0);
}

void UCombatPresenceComponent::ApplyPolicy()
{
    AActor* Owner = GetOwner();
    if (!Owner) { return; }

    switch (DefaultPolicy)
    {
        case ECombatPresencePolicy::Ignore:
        case ECombatPresencePolicy::BecomeObstacle:
            // Both leave the actor untouched in Phase 1. BecomeObstacle is kept
            // as a distinct policy for semantic clarity and so that the
            // future cover system can pick it up without reclassifying.
            break;

        case ECombatPresencePolicy::HideAndPause:
            Owner->SetActorHiddenInGame(true);
            Owner->SetActorEnableCollision(false);
            Owner->SetActorTickEnabled(false);

            UE_LOG(LogTemp, Verbose,
                TEXT("[CombatPresenceComponent] ApplyPolicy HideAndPause on '%s'"),
                *Owner->GetName());
            break;
    }
}

void UCombatPresenceComponent::RestoreFromSnapshot()
{
    AActor* Owner = GetOwner();

    if (!Owner || !bSnapshotValid)
    {
        // Snapshot was either never captured or was explicitly invalidated by
        // the actor (authority hand-off). Skip — the actor's own lifecycle is
        // the source of truth.
        bSnapshotValid = false;
        return;
    }

    Owner->SetActorHiddenInGame(bSnapshot_Hidden);
    Owner->SetActorEnableCollision(bSnapshot_CollisionEnabled);
    Owner->SetActorTickEnabled(bSnapshot_TickEnabled);

    UE_LOG(LogTemp, Verbose,
        TEXT("[CombatPresenceComponent] RestoreFromSnapshot on '%s': hidden=%d collision=%d tick=%d"),
        *Owner->GetName(),
        bSnapshot_Hidden ? 1 : 0,
        bSnapshot_CollisionEnabled ? 1 : 0,
        bSnapshot_TickEnabled ? 1 : 0);

    bSnapshotValid = false;
}

void UCombatPresenceComponent::InvalidateSnapshot()
{
    if (bSnapshotValid)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[CombatPresenceComponent] InvalidateSnapshot on '%s' — actor took authority over its combat presentation."),
            *GetNameSafe(GetOwner()));
    }

    bSnapshotValid = false;
}

UCombatActorScopeSubsystem* UCombatPresenceComponent::GetScopeSubsystem() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UCombatActorScopeSubsystem>() : nullptr;
}
