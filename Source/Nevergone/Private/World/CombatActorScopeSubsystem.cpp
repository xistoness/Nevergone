// Copyright Xyzto Works

#include "World/CombatActorScopeSubsystem.h"

#include "ActorComponents/CombatPresenceComponent.h"
#include "GameInstance/GameContextManager.h"
#include "Types/LevelTypes.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"

bool UCombatActorScopeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (!Super::ShouldCreateSubsystem(Outer))
    {
        return false;
    }

    // UGameContextManager is a GameInstanceSubsystem and a world subsystem cannot
    // declare a dependency across collections. Instead, only exist in worlds the
    // GameInstance already owns: UGameInstance::InitializeStandalone() builds the
    // placeholder "Untitled" world (and its subsystems) before calling Init(),
    // i.e. before the GameInstance subsystems exist. Editor worlds have no
    // GameInstance either and never need combat presence policies.
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->IsGameWorld() && World->GetGameInstance() != nullptr;
}

void UCombatActorScopeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UGameContextManager* CtxMgr = GetGameContextManager();
    if (!CtxMgr)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[CombatActorScopeSubsystem] Initialize: GameContextManager is null — combat presence policies will not be applied."));
        return;
    }

    OnGameContextChangedHandle = CtxMgr->OnGameContextChanged.AddUObject(
        this,
        &UCombatActorScopeSubsystem::HandleGameContextChanged
    );

    // Mirror the manager's current state. This covers a load-into-combat scenario:
    // when a saved game is restored mid-combat, GameContextManager may already
    // know it will enter Battle before this subsystem subscribes — but the
    // OnGameContextChanged broadcast that takes us into Battle still fires
    // afterwards (driven by TowerFloorGameMode::BeginPlay → RequestInitialState),
    // so registration timing is safe either way.
    bInCombatPresenceMode = IsCombatState(CtxMgr->GetCurrentState());

    UE_LOG(LogTemp, Log,
        TEXT("[CombatActorScopeSubsystem] Initialize: subscribed. InitialState=%d bInCombatPresenceMode=%d"),
        (int32)CtxMgr->GetCurrentState(),
        bInCombatPresenceMode ? 1 : 0);
}

void UCombatActorScopeSubsystem::Deinitialize()
{
    if (UGameContextManager* CtxMgr = GetGameContextManager())
    {
        CtxMgr->OnGameContextChanged.Remove(OnGameContextChangedHandle);

        UE_LOG(LogTemp, Log,
            TEXT("[CombatActorScopeSubsystem] Deinitialize: unsubscribed from OnGameContextChanged."));
    }

    OnGameContextChangedHandle.Reset();
    RegisteredComponents.Reset();
    bInCombatPresenceMode = false;

    Super::Deinitialize();
}

void UCombatActorScopeSubsystem::RegisterPresenceComponent(UCombatPresenceComponent* Component)
{
    if (!Component) { return; }

    RegisteredComponents.AddUnique(Component);

    UE_LOG(LogTemp, Log,
        TEXT("[CombatActorScopeSubsystem] Register '%s' (policy=%d, inCombat=%d). Total=%d"),
        *GetNameSafe(Component->GetOwner()),
        (int32)Component->DefaultPolicy,
        bInCombatPresenceMode ? 1 : 0,
        RegisteredComponents.Num());

    // If we are already in a combat span, apply policy immediately so the
    // newly-spawned actor does not flash visible for a frame.
    if (bInCombatPresenceMode)
    {
        Component->CaptureSnapshot();
        Component->ApplyPolicy();
    }
}

void UCombatActorScopeSubsystem::UnregisterPresenceComponent(UCombatPresenceComponent* Component)
{
    if (!Component) { return; }

    const int32 Removed = RegisteredComponents.RemoveSingleSwap(Component);

    UE_LOG(LogTemp, Log,
        TEXT("[CombatActorScopeSubsystem] Unregister '%s' (removed=%d, inCombat=%d). Total=%d"),
        *GetNameSafe(Component->GetOwner()),
        Removed,
        bInCombatPresenceMode ? 1 : 0,
        RegisteredComponents.Num());
}

void UCombatActorScopeSubsystem::HandleGameContextChanged(EGameContextState NewState)
{
    const bool bWasInCombat  = bInCombatPresenceMode;
    const bool bWillBeInCombat = IsCombatState(NewState);

    UE_LOG(LogTemp, Log,
        TEXT("[CombatActorScopeSubsystem] HandleGameContextChanged: NewState=%d wasInCombat=%d willBeInCombat=%d"),
        (int32)NewState,
        bWasInCombat ? 1 : 0,
        bWillBeInCombat ? 1 : 0);

    if (!bWasInCombat && bWillBeInCombat)
    {
        EnterCombatPresenceMode();
    }
    else if (bWasInCombat && !bWillBeInCombat)
    {
        ExitCombatPresenceMode();
    }
    // else: same span on both sides — nothing to do.
}

bool UCombatActorScopeSubsystem::IsCombatState(EGameContextState State)
{
    return State == EGameContextState::BattlePreparation
        || State == EGameContextState::Battle
        || State == EGameContextState::BattleResults;
}

void UCombatActorScopeSubsystem::EnterCombatPresenceMode()
{
    bInCombatPresenceMode = true;

    int32 AppliedCount = 0;
    int32 PrunedStaleCount = 0;

    for (auto It = RegisteredComponents.CreateIterator(); It; ++It)
    {
        UCombatPresenceComponent* Comp = It->Get();
        if (!Comp)
        {
            It.RemoveCurrent();
            ++PrunedStaleCount;
            continue;
        }

        Comp->CaptureSnapshot();
        Comp->ApplyPolicy();
        ++AppliedCount;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[CombatActorScopeSubsystem] EnterCombatPresenceMode: applied policy to %d components (%d stale entries pruned)."),
        AppliedCount, PrunedStaleCount);
}

void UCombatActorScopeSubsystem::ExitCombatPresenceMode()
{
    bInCombatPresenceMode = false;

    int32 RestoredCount = 0;
    int32 SkippedInvalidatedCount = 0;
    int32 PrunedStaleCount = 0;

    for (auto It = RegisteredComponents.CreateIterator(); It; ++It)
    {
        UCombatPresenceComponent* Comp = It->Get();
        if (!Comp)
        {
            It.RemoveCurrent();
            ++PrunedStaleCount;
            continue;
        }

        if (Comp->IsSnapshotValid())
        {
            Comp->RestoreFromSnapshot();
            ++RestoredCount;
        }
        else
        {
            // Snapshot was invalidated by the actor (authority hand-off, e.g.
            // FloorEncounterVolume::DeactivateEncounter on victory).
            ++SkippedInvalidatedCount;
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[CombatActorScopeSubsystem] ExitCombatPresenceMode: restored %d components, skipped %d (actor authority), pruned %d stale."),
        RestoredCount, SkippedInvalidatedCount, PrunedStaleCount);
}

UGameContextManager* UCombatActorScopeSubsystem::GetGameContextManager() const
{
    UWorld* World = GetWorld();
    if (!World) { return nullptr; }

    UGameInstance* GI = World->GetGameInstance();
    if (!GI) { return nullptr; }

    return GI->GetSubsystem<UGameContextManager>();
}
