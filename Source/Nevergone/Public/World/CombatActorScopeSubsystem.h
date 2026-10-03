// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatActorScopeSubsystem.generated.h"

class UCombatPresenceComponent;
class UGameContextManager;
enum class EGameContextState : uint8;

// ---------------------------------------------------------------------------
// UCombatActorScopeSubsystem
//
// Per-world subsystem that drives the visibility, collision, and tick state
// of "exploration-only" actors during combat. Listens to
// UGameContextManager::OnGameContextChanged and applies each registered
// UCombatPresenceComponent's policy on transition.
//
// Combat span:
//   "Combat" covers BattlePreparation, Battle, and BattleResults.
//   The snapshot is captured ONCE on entering the span and restored ONCE on
//   leaving it. Transitions inside the span (Prep→Battle, Battle→Results)
//   do not re-apply policy and do not clobber the snapshot.
//
// Mid-combat spawns:
//   Actors whose BeginPlay fires during a combat span register here and have
//   policy applied immediately, so they appear already-hidden / paused
//   without a frame of flicker.
//
// Lifetime choice (UWorldSubsystem, not UGameInstanceSubsystem):
//   The registered set is inherently per-world — level travel resets it.
//   UGameContextManager is a GameInstanceSubsystem and persists across travels,
//   so this subsystem subscribes/unsubscribes to its delegate in
//   Initialize/Deinitialize.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UCombatActorScopeSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:

    // -----------------------------------------------------------------------
    // UWorldSubsystem
    // -----------------------------------------------------------------------
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // -----------------------------------------------------------------------
    // Component registration — called by UCombatPresenceComponent
    // -----------------------------------------------------------------------

    /**
     * Registers a presence component. If the world is currently in a combat
     * span (mid-combat spawn), the component's snapshot is captured and
     * policy applied immediately to avoid a frame of flicker.
     */
    void RegisterPresenceComponent(UCombatPresenceComponent* Component);

    /** Unregisters a presence component. No restore is performed. */
    void UnregisterPresenceComponent(UCombatPresenceComponent* Component);

    bool IsInCombatPresenceMode() const { return bInCombatPresenceMode; }

private:

    void HandleGameContextChanged(EGameContextState NewState);

    static bool IsCombatState(EGameContextState State);

    void EnterCombatPresenceMode();
    void ExitCombatPresenceMode();

    UGameContextManager* GetGameContextManager() const;

    // Weak pointers so destroyed actors do not leave dangling references.
    // Stale entries are pruned during apply / restore loops.
    TArray<TWeakObjectPtr<UCombatPresenceComponent>> RegisteredComponents;

    // True while the world is in BattlePreparation, Battle, or BattleResults.
    bool bInCombatPresenceMode = false;

    FDelegateHandle OnGameContextChangedHandle;
};
