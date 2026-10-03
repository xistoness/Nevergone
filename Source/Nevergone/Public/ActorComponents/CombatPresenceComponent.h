// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatPresenceComponent.generated.h"

class UCombatActorScopeSubsystem;

// ---------------------------------------------------------------------------
// ECombatPresencePolicy
//
// Declares how an actor reacts to entering/leaving the combat span
// (BattlePreparation, Battle, BattleResults).
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class ECombatPresencePolicy : uint8
{
    /**
     * No-op policy. Used as an explicit override for class defaults — e.g. a
     * specific NPC instance that should remain visible during combat (a chained
     * prisoner, a boss who triggered the fight). Equivalent to having no
     * UCombatPresenceComponent at all.
     */
    Ignore         UMETA(DisplayName = "Ignore"),

    /**
     * Hides visibility, disables collision, and disables tick while combat is
     * active. Default for animated/civilian actors that break tactical
     * legibility (dialogue NPCs, inactive encounter volumes, debug actors).
     */
    HideAndPause   UMETA(DisplayName = "Hide and Pause"),

    /**
     * Stays visible and collidable as scenery during combat. Interaction is
     * implicitly disabled because the exploration interaction trace is dormant
     * while UExplorationModeComponent is not ticking.
     *
     * Phase 1 note: Most prop actors (containers, world items, doors) only
     * have collision on a small InteractionSphere or DoorCollisionBox.
     * Containers and world items therefore do NOT block grid tiles today —
     * their interaction sphere ignores ECC_Visibility, which is what the
     * grid generator traces with. True tactical-cover behavior is deferred
     * to the cover system pass (Phase 3).
     *
     * Today, BecomeObstacle is behaviorally identical to Ignore but is kept
     * as a distinct policy for semantic clarity and forward-compatibility
     * with the cover system.
     */
    BecomeObstacle UMETA(DisplayName = "Become Obstacle"),
};

// ---------------------------------------------------------------------------
// UCombatPresenceComponent
//
// Tag component declaring how an actor should behave when the world enters
// or leaves combat. Auto-registers with UCombatActorScopeSubsystem on
// BeginPlay and unregisters on EndPlay.
//
// Snapshot model:
//   On entering the combat span, the subsystem calls CaptureSnapshot to
//   record the actor's current Hidden / Collision / Tick state, then calls
//   ApplyPolicy to mutate it according to DefaultPolicy.
//   On leaving the combat span, the subsystem calls RestoreFromSnapshot,
//   which writes the captured values back verbatim — so any other system
//   that legitimately changes those flags during exploration (quests,
//   narrative beats) composes correctly.
//
// Authority hand-off:
//   When the owning actor's own lifecycle takes authoritative ownership of
//   its presentation state during combat (e.g. AFloorEncounterVolume
//   permanently deactivating after a victory), it calls InvalidateSnapshot
//   so the subsystem will NOT clobber that state on combat exit.
// ---------------------------------------------------------------------------
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class NEVERGONE_API UCombatPresenceComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UCombatPresenceComponent();

    /**
     * The policy applied to the owning actor when combat starts.
     * Each owning actor sets this in its constructor to declare a class
     * default; level designers may override per-instance via the editor.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Presence")
    ECombatPresencePolicy DefaultPolicy = ECombatPresencePolicy::HideAndPause;

    // -----------------------------------------------------------------------
    // Subsystem-facing API
    // -----------------------------------------------------------------------

    /** Records the actor's current Hidden / Collision / Tick state. */
    void CaptureSnapshot();

    /** Mutates the actor according to DefaultPolicy. */
    void ApplyPolicy();

    /** Writes the snapshot back onto the actor and clears it. */
    void RestoreFromSnapshot();

    /** Returns true if a snapshot has been captured and not yet restored. */
    bool IsSnapshotValid() const { return bSnapshotValid; }

    // -----------------------------------------------------------------------
    // Actor authority hand-off
    // -----------------------------------------------------------------------

    /**
     * Called by the owning actor when it takes authoritative ownership of its
     * presentation state during combat — for example AFloorEncounterVolume
     * permanently deactivating after a victory. After this, RestoreFromSnapshot
     * becomes a no-op so the subsystem will not undo the actor's own changes.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Presence")
    void InvalidateSnapshot();

protected:

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

    UCombatActorScopeSubsystem* GetScopeSubsystem() const;

    // Pre-combat snapshot. Only valid between EnterCombatPresenceMode and
    // ExitCombatPresenceMode on the subsystem.
    bool bSnapshotValid = false;
    bool bSnapshot_Hidden = false;
    bool bSnapshot_CollisionEnabled = true;
    bool bSnapshot_TickEnabled = false;
};
