// Copyright Xyzto Works

#include "GameMode/Combat/StatusEffectManager.h"

#include "AbilitySystemComponent.h"
#include "Characters/CharacterBase.h"
#include "Data/StatusEffectDefinition.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/BattleUnitState.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "GameMode/TurnManager.h"
#include "Nevergone.h"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UStatusEffectManager::Initialize(
    UBattleState*    InBattleState,
    UCombatEventBus* InEventBus,
    UTurnManager*    InTurnManager
)
{
    BattleState = InBattleState;
    EventBus    = InEventBus;
    TurnManager = InTurnManager;

    TurnStateHandle = TurnManager->OnTurnStateChanged.AddUObject(
        this, &UStatusEffectManager::OnTurnStateChanged
    );

    UE_LOG(LogNevergone, Log, TEXT("[StatusEffectManager] Initialized"));
}

void UStatusEffectManager::Shutdown()
{
    if (TurnStateHandle.IsValid() && TurnManager)
    {
        TurnManager->OnTurnStateChanged.Remove(TurnStateHandle);
        TurnStateHandle.Reset();
    }

    UE_LOG(LogNevergone, Log, TEXT("[StatusEffectManager] Shutdown"));
}

// ---------------------------------------------------------------------------
// Application
// ---------------------------------------------------------------------------

void UStatusEffectManager::ApplyStatusEffect(
    ACharacterBase*          Caster,
    ACharacterBase*          Target,
    UStatusEffectDefinition* Definition,
    int32                    CasterStatSnapshot
)
{
    if (!Target || !Definition)
    {
        UE_LOG(LogNevergone, Warning,
            TEXT("[StatusEffectManager] ApplyStatusEffect: null Target or Definition — skipped"));
        return;
    }

    FBattleUnitState* TargetState = BattleState->FindUnitState(Target);
    if (!TargetState || TargetState->bIsDead)
    {
        UE_LOG(LogNevergone, Warning,
            TEXT("[StatusEffectManager] ApplyStatusEffect: no live BattleUnitState for %s — skipped"),
            *GetNameSafe(Target));
        return;
    }

    if (IsImmune(Target, Definition))
    {
        UE_LOG(LogNevergone, Log,
            TEXT("[StatusEffectManager] %s is immune to '%s' — skipped"),
            *GetNameSafe(Target), *Definition->DisplayName.ToString());
        return;
    }

    const FGameplayTag& Tag = Definition->StatusTag;

    // --- Handle stack behavior for existing instances ---
    const int32 ExistingIndex = TargetState->ActiveStatusEffects.IndexOfByPredicate(
        [&Tag](const FActiveStatusEffect& E)
        {
            return E.Definition && E.Definition->StatusTag == Tag;
        }
    );

    const bool bHasExisting = ExistingIndex != INDEX_NONE;

    if (bHasExisting)
    {
        switch (Definition->StackBehavior)
        {
            case EStatusStackBehavior::Ignore:
                UE_LOG(LogNevergone, Log,
                    TEXT("[StatusEffectManager] '%s' on %s already active — ignoring re-application"),
                    *Tag.ToString(), *GetNameSafe(Target));
                return;

            case EStatusStackBehavior::Refresh:
                TargetState->ActiveStatusEffects[ExistingIndex].TurnsRemaining = Definition->DurationTurns;
                UE_LOG(LogNevergone, Log,
                    TEXT("[StatusEffectManager] '%s' on %s refreshed to %d turns"),
                    *Tag.ToString(), *GetNameSafe(Target), Definition->DurationTurns);
                return;

            case EStatusStackBehavior::Stack:
                // Fall through to create a new independent instance
                break;
        }
    }

    // --- Build the new instance ---
    FActiveStatusEffect NewInstance;
    NewInstance.Definition          = Definition;
    NewInstance.Caster              = Caster;
    NewInstance.TurnsRemaining      = Definition->DurationTurns;
    NewInstance.InstanceId          = GenerateInstanceId();
    // CachedCasterStatValue is set by the caller (the ability), not inferred here.
    // This keeps the Definition independent of stat-source knowledge.
    NewInstance.CachedCasterStatValue = CasterStatSnapshot;

    if (Caster)
    {
        const FBattleUnitState* CasterState = BattleState->FindUnitState(Caster);
        if (CasterState)
        {
            NewInstance.CasterTeam = CasterState->Team;
        }
    }

    // Add to list before ApplyPassiveEffect so Shield can read CachedCasterStatValue
    TargetState->ActiveStatusEffects.Add(NewInstance);
    FActiveStatusEffect& StoredInstance = TargetState->ActiveStatusEffects.Last();

    // --- Apply passive (stat mod, shield, charm, etc.) ---
    ApplyPassiveEffect(Target, StoredInstance);

    // --- Grant ASC tag (first instance only — tag is per-unit, not per-stack) ---
    if (!bHasExisting && Tag.IsValid())
    {
        if (UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent())
        {
            ASC->AddLooseGameplayTags(FGameplayTagContainer(Tag));
            UE_LOG(LogNevergone, Log,
                TEXT("[StatusEffectManager] ASC tag '%s' granted to %s"),
                *Tag.ToString(), *GetNameSafe(Target));
        }
    }

    // --- Notify event bus (floating text, audio, HP bar icon delegates) ---
    if (EventBus)
    {
        EventBus->NotifyStatusApplied(Target, Tag, Definition->DisplayLabel, Definition->Icon);
    }

    UE_LOG(LogNevergone, Log,
        TEXT("[StatusEffectManager] Applied '%s' to %s by %s — %d turns, casterStat=%d, instanceId=%d"),
        *Tag.ToString(), *GetNameSafe(Target), *GetNameSafe(Caster),
        Definition->DurationTurns, CasterStatSnapshot, StoredInstance.InstanceId);

    // --- Immediate tick on application if configured ---
    // Allows DoT statuses like Poison to deal damage immediately on the caster's turn
    // rather than waiting a full cycle.
    if (Definition->bTickOnApplication
        && Definition->TickEffect != EStatusTickEffect::None
        && Definition->TickTiming != EStatusTickTiming::None)
    {
        UE_LOG(LogNevergone, Log,
            TEXT("[StatusEffectManager] bTickOnApplication=true for '%s' on %s — ticking immediately"),
            *Tag.ToString(), *GetNameSafe(Target));
        TickEffectInstance(Target, StoredInstance);
    }
}

// ---------------------------------------------------------------------------
// Removal
// ---------------------------------------------------------------------------

void UStatusEffectManager::RemoveStatusEffect(ACharacterBase* Target, const FGameplayTag& StatusTag)
{
    if (!Target) { return; }

    FBattleUnitState* TargetState = BattleState->FindUnitState(Target);
    if (!TargetState) { return; }

    TArray<int32> IdsToRemove;
    for (const FActiveStatusEffect& Instance : TargetState->ActiveStatusEffects)
    {
        if (Instance.Definition && Instance.Definition->StatusTag == StatusTag)
        {
            IdsToRemove.Add(Instance.InstanceId);
        }
    }

    if (IdsToRemove.IsEmpty())
    {
        UE_LOG(LogNevergone, Verbose,
            TEXT("[StatusEffectManager] RemoveStatusEffect: '%s' not found on %s"),
            *StatusTag.ToString(), *GetNameSafe(Target));
        return;
    }

    for (int32 Id : IdsToRemove)
    {
        RemoveStatusEffectInstance(Target, Id);
    }

    UE_LOG(LogNevergone, Log,
        TEXT("[StatusEffectManager] Removed %d instance(s) of '%s' from %s"),
        IdsToRemove.Num(), *StatusTag.ToString(), *GetNameSafe(Target));
}

void UStatusEffectManager::RemoveStatusEffectInstance(ACharacterBase* Target, int32 InstanceId)
{
    if (!Target) { return; }

    FBattleUnitState* TargetState = BattleState->FindUnitState(Target);
    if (!TargetState) { return; }

    const int32 Index = TargetState->ActiveStatusEffects.IndexOfByPredicate(
        [InstanceId](const FActiveStatusEffect& E) { return E.InstanceId == InstanceId; }
    );

    if (Index == INDEX_NONE)
    {
        UE_LOG(LogNevergone, Warning,
            TEXT("[StatusEffectManager] RemoveStatusEffectInstance: id=%d not found on %s"),
            InstanceId, *GetNameSafe(Target));
        return;
    }

    const FActiveStatusEffect InstanceCopy = TargetState->ActiveStatusEffects[Index];
    const FGameplayTag Tag = InstanceCopy.Definition
        ? InstanceCopy.Definition->StatusTag
        : FGameplayTag();

    RevertPassiveEffect(Target, InstanceCopy);
    TargetState->ActiveStatusEffects.RemoveAt(Index);

    // Remove the ASC tag only when no other instances of this status remain
    const bool bOtherInstancesExist = TargetState->ActiveStatusEffects.ContainsByPredicate(
        [&Tag](const FActiveStatusEffect& E)
        {
            return E.Definition && E.Definition->StatusTag == Tag;
        }
    );

    if (!bOtherInstancesExist && Tag.IsValid())
    {
        if (UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent())
        {
            ASC->RemoveLooseGameplayTags(FGameplayTagContainer(Tag));
        }

        if (EventBus)
        {
            EventBus->NotifyStatusCleared(Target, Tag);
        }

        UE_LOG(LogNevergone, Log,
            TEXT("[StatusEffectManager] '%s' fully cleared from %s"),
            *Tag.ToString(), *GetNameSafe(Target));
    }
}

// ---------------------------------------------------------------------------
// Shield
// ---------------------------------------------------------------------------

int32 UStatusEffectManager::AbsorbDamageWithShield(ACharacterBase* Target, int32 IncomingDamage)
{
    if (!Target || IncomingDamage <= 0) { return IncomingDamage; }

    FBattleUnitState* State = BattleState->FindUnitState(Target);
    if (!State) { return IncomingDamage; }

    int32 RemainingDamage = IncomingDamage;
    TArray<int32> DepletedIds;

    for (FActiveStatusEffect& Instance : State->ActiveStatusEffects)
    {
        if (Instance.ShieldHP <= 0 || RemainingDamage <= 0) { continue; }

        const int32 Absorbed = FMath::Min(Instance.ShieldHP, RemainingDamage);
        Instance.ShieldHP   -= Absorbed;
        RemainingDamage     -= Absorbed;

        UE_LOG(LogNevergone, Log,
            TEXT("[StatusEffectManager] Shield on %s absorbed %d — %d shield HP left, %d passed through"),
            *GetNameSafe(Target), Absorbed, Instance.ShieldHP, RemainingDamage);

        if (Instance.ShieldHP <= 0)
        {
            DepletedIds.Add(Instance.InstanceId);
        }
    }

    for (int32 Id : DepletedIds)
    {
        UE_LOG(LogNevergone, Log,
            TEXT("[StatusEffectManager] Shield depleted on %s (instanceId=%d)"),
            *GetNameSafe(Target), Id);
        RemoveStatusEffectInstance(Target, Id);
    }

    return RemainingDamage;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool UStatusEffectManager::HasStatusEffect(
    const ACharacterBase* Target,
    const FGameplayTag&   StatusTag
) const
{
    if (!Target || !BattleState) { return false; }

    const FBattleUnitState* State =
        BattleState->FindUnitState(const_cast<ACharacterBase*>(Target));
    if (!State) { return false; }

    return State->ActiveStatusEffects.ContainsByPredicate(
        [&StatusTag](const FActiveStatusEffect& E)
        {
            return E.Definition && E.Definition->StatusTag == StatusTag;
        }
    );
}

int32 UStatusEffectManager::GetShieldHP(const ACharacterBase* Target) const
{
    if (!Target || !BattleState) { return 0; }

    const FBattleUnitState* State =
        BattleState->FindUnitState(const_cast<ACharacterBase*>(Target));
    if (!State) { return 0; }

    int32 Total = 0;
    for (const FActiveStatusEffect& Instance : State->ActiveStatusEffects)
    {
        Total += Instance.ShieldHP;
    }
    return Total;
}

// ---------------------------------------------------------------------------
// Turn tick
// ---------------------------------------------------------------------------

void UStatusEffectManager::OnTurnStateChanged(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase)
{
    const bool bIsPlayerTurnStart =
        (NewOwner == EBattleTurnOwner::Player && NewPhase == EBattleTurnPhase::AwaitingOrders);
    const bool bIsEnemyTurnStart =
        (NewOwner == EBattleTurnOwner::Enemy && NewPhase == EBattleTurnPhase::ExecutingActions);

    // EndOf timings fire when the OTHER team's turn starts, which equals the end of this team's turn.
    // Player EndOf  → fires when Enemy turn starts
    // Enemy  EndOf  → fires when Player turn starts
    const bool bIsPlayerTurnEnd = bIsEnemyTurnStart;
    const bool bIsEnemyTurnEnd  = bIsPlayerTurnStart;

    if (!bIsPlayerTurnStart && !bIsEnemyTurnStart) { return; }

    const EBattleUnitTeam ActiveTeam =
        bIsPlayerTurnStart ? EBattleUnitTeam::Ally : EBattleUnitTeam::Enemy;

    UE_LOG(LogNevergone, Log,
        TEXT("[StatusEffectManager] Turn event — ticking statuses (team=%s, playerStart=%s, enemyStart=%s)"),
        ActiveTeam == EBattleUnitTeam::Ally ? TEXT("Ally") : TEXT("Enemy"),
        bIsPlayerTurnStart ? TEXT("true") : TEXT("false"),
        bIsEnemyTurnStart  ? TEXT("true") : TEXT("false"));

    // --- StartOf ticks ---
    // Victim-timed: fire for units whose own turn is starting
    TickStatusesForTiming(EStatusTickTiming::StartOfVictimTurn, ActiveTeam);
    // Caster-timed: fire for effects whose caster's team turn is starting
    TickStatusesForTiming(EStatusTickTiming::StartOfCasterTurn, ActiveTeam);

    // --- EndOf ticks ---
    // These fire at the end of the OPPOSING team's turn, meaning:
    //   EndOfVictimTurn fires when enemy turn starts (victim = ally team just ending their turn)
    //   EndOfCasterTurn fires when enemy turn starts (caster = ally team just ending their turn)
    if (bIsPlayerTurnEnd)
    {
        // Ally team's turn just ended
        TickStatusesForTiming(EStatusTickTiming::EndOfVictimTurn, EBattleUnitTeam::Ally);
        TickStatusesForTiming(EStatusTickTiming::EndOfCasterTurn, EBattleUnitTeam::Ally);
    }
    if (bIsEnemyTurnEnd)
    {
        // Enemy team's turn just ended
        TickStatusesForTiming(EStatusTickTiming::EndOfVictimTurn, EBattleUnitTeam::Enemy);
        TickStatusesForTiming(EStatusTickTiming::EndOfCasterTurn, EBattleUnitTeam::Enemy);
    }

    // --- Expire stale statuses once per full round (player turn start only) ---
    // IMPORTANT: expiry runs AFTER all ticks so a status with TurnsRemaining == 1
    // fires its last tick before being removed. Otherwise the final tick would be lost.
    if (bIsPlayerTurnStart)
    {
        DecrementAndExpireStatuses();
    }
}

void UStatusEffectManager::TickStatusesForTiming(EStatusTickTiming Timing, EBattleUnitTeam ActiveTeam)
{
    if (!BattleState) { return; }

    // --- Phase 1: collect what needs to tick ---
    // We must NOT call TickEffectInstance inside the range-for because it calls
    // NotifyDamageApplied, which can kill the unit, which calls RemoveStatusEffectInstance,
    // which removes entries from ActiveStatusEffects — mutating the array mid-iteration
    // triggers an ensure failure and undefined behavior.
    struct FPendingTick
    {
        ACharacterBase* Target;
        int32           InstanceId;
    };
    TArray<FPendingTick> PendingTicks;

    for (const FBattleUnitState& UnitState : BattleState->GetAllUnitStates())
    {
        if (UnitState.bIsDead) { continue; }

        ACharacterBase* Unit = UnitState.UnitActor.Get();
        if (!Unit) { continue; }

        const FBattleUnitState* State = BattleState->FindUnitState(Unit);
        if (!State) { continue; }

        for (const FActiveStatusEffect& Instance : State->ActiveStatusEffects)
        {
            if (!Instance.IsValid()) { continue; }
            if (Instance.Definition->TickTiming != Timing) { continue; }
            if (Instance.Definition->TickEffect == EStatusTickEffect::None) { continue; }

            bool bShouldTick = false;

            switch (Timing)
            {
                case EStatusTickTiming::StartOfVictimTurn:
                case EStatusTickTiming::EndOfVictimTurn:
                    bShouldTick = (UnitState.Team == ActiveTeam);
                    break;

                case EStatusTickTiming::StartOfCasterTurn:
                case EStatusTickTiming::EndOfCasterTurn:
                    bShouldTick = (Instance.CasterTeam == ActiveTeam);
                    break;

                default:
                    break;
            }

            if (bShouldTick)
            {
                PendingTicks.Add({ Unit, Instance.InstanceId });
            }
        }
    }

    // --- Phase 2: execute ticks safely ---
    // Each tick is identified by InstanceId. Before executing, verify the instance
    // still exists — a previous tick in this same batch may have killed the unit
    // and triggered automatic status removal.
    for (const FPendingTick& Pending : PendingTicks)
    {
        if (!IsValid(Pending.Target)) { continue; }

        FBattleUnitState* MutableState = BattleState->FindUnitState(Pending.Target);
        if (!MutableState || MutableState->bIsDead) { continue; }

        // Find the instance by ID — it may have been removed by a prior tick this frame
        FActiveStatusEffect* Instance = MutableState->ActiveStatusEffects.FindByPredicate(
            [Id = Pending.InstanceId](const FActiveStatusEffect& E) { return E.InstanceId == Id; }
        );

        if (Instance)
        {
            TickEffectInstance(Pending.Target, *Instance);
        }
    }
}

void UStatusEffectManager::TickEffectInstance(ACharacterBase* Target, FActiveStatusEffect& Instance)
{
    if (!Instance.IsValid() || !EventBus) { return; }

    const float Amount = ResolveTickAmount(Target, Instance);
    if (Amount <= 0.f) { return; }

    const int32 IntAmount = FMath::Max(1, FMath::RoundToInt(Amount));

    switch (Instance.Definition->TickEffect)
    {
        case EStatusTickEffect::DamagePerTurn:
            UE_LOG(LogNevergone, Log,
                TEXT("[StatusEffectManager] Tick '%s': %d damage to %s"),
                *Instance.Definition->StatusTag.ToString(), IntAmount, *GetNameSafe(Target));
            EventBus->NotifyDamageApplied(Instance.Caster.Get(), Target, IntAmount);
            break;

        case EStatusTickEffect::HealPerTurn:
            UE_LOG(LogNevergone, Log,
                TEXT("[StatusEffectManager] Tick '%s': %d heal on %s"),
                *Instance.Definition->StatusTag.ToString(), IntAmount, *GetNameSafe(Target));
            EventBus->NotifyHealApplied(Instance.Caster.Get(), Target, IntAmount);
            break;

        default:
            break;
    }
}

void UStatusEffectManager::DecrementAndExpireStatuses()
{
    if (!BattleState) { return; }

    for (const FBattleUnitState& UnitState : BattleState->GetAllUnitStates())
    {
        ACharacterBase* Unit = UnitState.UnitActor.Get();
        if (!Unit) { continue; }

        FBattleUnitState* MutableState = BattleState->FindUnitState(Unit);
        if (!MutableState) { continue; }

        TArray<int32> ExpiredIds;

        for (FActiveStatusEffect& Instance : MutableState->ActiveStatusEffects)
        {
            if (!Instance.IsValid()) { continue; }

            // TurnsRemaining == 0 means permanent — never decrement
            if (Instance.TurnsRemaining == 0) { continue; }

            Instance.TurnsRemaining--;

            UE_LOG(LogNevergone, Verbose,
                TEXT("[StatusEffectManager] '%s' on %s — %d turns remaining"),
                *Instance.Definition->StatusTag.ToString(),
                *GetNameSafe(Unit),
                Instance.TurnsRemaining);

            if (Instance.TurnsRemaining <= 0)
            {
                ExpiredIds.Add(Instance.InstanceId);
            }
        }

        for (int32 Id : ExpiredIds)
        {
            UE_LOG(LogNevergone, Log,
                TEXT("[StatusEffectManager] Status expired on %s (instanceId=%d)"),
                *GetNameSafe(Unit), Id);
            RemoveStatusEffectInstance(Unit, Id);
        }
    }
}

// ---------------------------------------------------------------------------
// Passive effect helpers
// ---------------------------------------------------------------------------

void UStatusEffectManager::ApplyPassiveEffect(ACharacterBase* Target, FActiveStatusEffect& Instance)
{
    if (!Instance.IsValid() || !BattleState) { return; }

    FBattleUnitState* State = BattleState->FindUnitState(Target);
    if (!State) { return; }

    switch (Instance.Definition->PassiveEffect)
    {
        case EStatusPassiveEffect::ReduceMovementRangePercent:
        {
            // Penalty = Floor(CurrentMovementRange * PassiveEffectTargetMultiplier)
            // Using CurrentMovementRange so that stacked slows each bite off the already-reduced value.
            const int32 Penalty = FMath::Max(1, FMath::FloorToInt(
                State->MovementRange * Instance.Definition->PassiveEffectTargetMultiplier
            ));
            State->MovementRange = FMath::Max(1, State->MovementRange - Penalty);

            UE_LOG(LogNevergone, Log,
                TEXT("[StatusEffectManager] Slow on %s — MovementRange reduced by %d (now %d)"),
                *GetNameSafe(Target), Penalty, State->MovementRange);
            break;
        }

        case EStatusPassiveEffect::IncreaseMovementRangePercent:
        {
            // Bonus = Floor(CurrentMovementRange * PassiveEffectTargetMultiplier)
            const int32 Bonus = FMath::Max(1, FMath::FloorToInt(
                State->MovementRange * Instance.Definition->PassiveEffectTargetMultiplier
            ));
            State->MovementRange += Bonus;

            UE_LOG(LogNevergone, Log,
                TEXT("[StatusEffectManager] Haste on %s — MovementRange increased by %d (now %d)"),
                *GetNameSafe(Target), Bonus, State->MovementRange);
            break;
        }

        case EStatusPassiveEffect::Shield:
        {
            // ShieldHP = PassiveEffectFlatAmount + Floor(CachedCasterStatValue * PassiveEffectCasterMultiplier)
            // CachedCasterStatValue was snapshotted by the ability at application time.
            const int32 CasterContrib = FMath::FloorToInt(
                Instance.CachedCasterStatValue * Instance.Definition->PassiveEffectCasterMultiplier
            );
            Instance.ShieldHP = FMath::Max(1,
                Instance.Definition->PassiveEffectFlatAmount + CasterContrib
            );

            UE_LOG(LogNevergone, Log,
                TEXT("[StatusEffectManager] Shield on %s — %d HP (flat=%d, casterStat=%d * mult=%.2f)"),
                *GetNameSafe(Target), Instance.ShieldHP,
                Instance.Definition->PassiveEffectFlatAmount,
                Instance.CachedCasterStatValue,
                Instance.Definition->PassiveEffectCasterMultiplier);
            break;
        }

        case EStatusPassiveEffect::GrantActionPoints:
        {
            // AP += PassiveEffectFlatAmount
            State->CurrentActionPoints += Instance.Definition->PassiveEffectFlatAmount;

            UE_LOG(LogNevergone, Log,
                TEXT("[StatusEffectManager] Respite on %s — +%d AP (now %d)"),
                *GetNameSafe(Target), Instance.Definition->PassiveEffectFlatAmount,
                State->CurrentActionPoints);
            break;
        }

        case EStatusPassiveEffect::CleansesStatuses:
            ApplyCleanse(Target, Instance);
            break;

        case EStatusPassiveEffect::SwitchTeam:
        {
            // Charm: flip the unit's BattleState team so TurnManager and AI include it correctly.
            const EBattleUnitTeam Original = State->Team;
            State->Team = (Original == EBattleUnitTeam::Ally)
                ? EBattleUnitTeam::Enemy
                : EBattleUnitTeam::Ally;

            UE_LOG(LogNevergone, Log,
                TEXT("[StatusEffectManager] Charm on %s — team flipped %s -> %s"),
                *GetNameSafe(Target),
                Original     == EBattleUnitTeam::Ally ? TEXT("Ally") : TEXT("Enemy"),
                State->Team  == EBattleUnitTeam::Ally ? TEXT("Ally") : TEXT("Enemy"));
            break;
        }

        default:
            break;
    }
}

void UStatusEffectManager::RevertPassiveEffect(
    ACharacterBase*             Target,
    const FActiveStatusEffect&  Instance
)
{
    if (!Instance.IsValid() || !BattleState) { return; }

    FBattleUnitState* State = BattleState->FindUnitState(Target);
    if (!State) { return; }

    switch (Instance.Definition->PassiveEffect)
    {
        case EStatusPassiveEffect::ReduceMovementRangePercent:
        {
            // Reverse: divide by (1 - percent) to recover the pre-penalty value exactly.
            const float Pct     = Instance.Definition->PassiveEffectTargetMultiplier;
            const float Divisor = FMath::Max(0.01f, 1.f - Pct);
            State->MovementRange = FMath::RoundToInt(State->MovementRange / Divisor);

            UE_LOG(LogNevergone, Log,
                TEXT("[StatusEffectManager] Slow removed from %s — MovementRange restored to %d"),
                *GetNameSafe(Target), State->MovementRange);
            break;
        }

        case EStatusPassiveEffect::IncreaseMovementRangePercent:
        {
            const float Pct     = Instance.Definition->PassiveEffectTargetMultiplier;
            const float Divisor = 1.f + Pct;
            State->MovementRange = FMath::RoundToInt(State->MovementRange / Divisor);

            UE_LOG(LogNevergone, Log,
                TEXT("[StatusEffectManager] Haste removed from %s — MovementRange restored to %d"),
                *GetNameSafe(Target), State->MovementRange);
            break;
        }

        case EStatusPassiveEffect::SwitchTeam:
        {
            const EBattleUnitTeam Current = State->Team;
            State->Team = (Current == EBattleUnitTeam::Ally)
                ? EBattleUnitTeam::Enemy
                : EBattleUnitTeam::Ally;

            UE_LOG(LogNevergone, Log,
                TEXT("[StatusEffectManager] Charm removed from %s — team restored to %s"),
                *GetNameSafe(Target),
                State->Team == EBattleUnitTeam::Ally ? TEXT("Ally") : TEXT("Enemy"));
            break;
        }

        // Shield: HP already zeroed by AbsorbDamageWithShield or natural expiry — nothing to revert.
        // GrantActionPoints: AP was consumed during the turn; not taken back on expiry by design.
        // CleansesStatuses: one-shot effect; nothing to revert.
        default:
            break;
    }
}

void UStatusEffectManager::ApplyCleanse(ACharacterBase* Target, FActiveStatusEffect& Instance)
{
    if (!Instance.IsValid()) { return; }

    UE_LOG(LogNevergone, Log,
        TEXT("[StatusEffectManager] Cure on %s — cleansing %d tag(s)"),
        *GetNameSafe(Target), Instance.Definition->TagsToCleanse.Num());

    for (const FGameplayTag& Tag : Instance.Definition->TagsToCleanse)
    {
        RemoveStatusEffect(Target, Tag);
    }

    // Force immediate expiry: set TurnsRemaining to 1 so DecrementAndExpireStatuses
    // removes this Cure instance at the end of the current turn cycle.
    Instance.TurnsRemaining = 1;
}

// ---------------------------------------------------------------------------
// Immunity
// ---------------------------------------------------------------------------

bool UStatusEffectManager::IsImmune(
    ACharacterBase*          Target,
    const UStatusEffectDefinition* Definition
) const
{
    if (!Target || !Definition) { return false; }

    const UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent();
    if (!ASC) { return false; }

    // Immunity tag convention: "Immunity." + status tag name with "." replaced by "_"
    // Example: State.Stunned -> Immunity.State_Stunned
    // Any system (UnitDefinition InnateImmunityTags, equipment GEs, terrain) can grant these.
    const FString TagName = Definition->StatusTag.GetTagName().ToString();
    const FString ImmunityName = FString::Printf(TEXT("Immunity.%s"),
        *TagName.Replace(TEXT("."), TEXT("_")));

    const FGameplayTag ImmunityTag = FGameplayTag::RequestGameplayTag(
        FName(*ImmunityName), /*bErrorIfNotFound=*/false
    );

    return ImmunityTag.IsValid() && ASC->HasMatchingGameplayTag(ImmunityTag);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

float UStatusEffectManager::ResolveTickAmount(
    const ACharacterBase*      Target,
    const FActiveStatusEffect& Instance
) const
{
    if (!Instance.IsValid() || !BattleState) { return 0.f; }

    switch (Instance.Definition->TickFormula)
    {
        case EStatusTickFormula::Flat:
            return Instance.Definition->TickFlatAmount;

        case EStatusTickFormula::PercentOfTargetMaxHP:
        {
            const FBattleUnitState* TargetState =
                BattleState->FindUnitState(const_cast<ACharacterBase*>(Target));
            return TargetState
                ? TargetState->MaxHP * Instance.Definition->TickPercent
                : 0.f;
        }

        case EStatusTickFormula::PercentOfCasterStat:
            return Instance.CachedCasterStatValue * Instance.Definition->TickPercent;
    }

    return 0.f;
}

int32 UStatusEffectManager::GenerateInstanceId()
{
    return NextInstanceId++;
}