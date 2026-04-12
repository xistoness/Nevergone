// Copyright Xyzto Works

#include "GameMode/Combat/BattleState.h"

#include "AbilitySystemComponent.h"
#include "ActorComponents/SaveableComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Data/StatusEffectDefinition.h"
#include "Data/UnitDefinition.h"
#include "GameInstance/MySaveGame.h"
#include "GameMode/BattlePreparationContext.h"
#include "Nevergone.h"

void UBattleState::Initialize(UBattlePreparationContext& BattlePrepContext)
{
    UnitStates.Empty();

    for (const FSpawnedBattleUnit& Spawned : BattlePrepContext.SpawnedUnits)
    {
        if (!Spawned.UnitActor.IsValid())
        {
            continue;
        }

        ACharacterBase* Unit = Spawned.UnitActor.Get();
        UUnitStatsComponent* Stats = Unit->GetUnitStats();

        if (Stats)
        {
            Stats->InitializeForBattle();
            // Clear any leftover temporary bonuses from a previous battle.
            Stats->ResetTemporaryBonuses();
        }

        FBattleUnitState NewState;
        NewState.UnitActor = Unit;
        NewState.Team      = Spawned.Team;

        if (Stats)
        {
            // Populate all derived stats via the shared helper so the logic
            // lives in one place — both fresh starts and restores use it.
            Stats->RecalculateBattleStats(NewState);
            NewState.MaxHP               = Stats->GetMaxHP();
            NewState.CurrentHP           = Stats->GetCurrentHP();
            NewState.CurrentActionPoints = Stats->GetActionPoints();
            NewState.TraversalParams     = Stats->GetTraversalParams();
        }

        // Propagate innate immunity tags from UnitDefinition to the ASC.
        if (Stats)
        {
            if (const UUnitDefinition* UnitDef = Stats->GetDefinition())
            {
                if (!UnitDef->InnateImmunityTags.IsEmpty())
                {
                    if (UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent())
                    {
                        ASC->AddLooseGameplayTags(UnitDef->InnateImmunityTags);

                        UE_LOG(LogTemp, Log,
                            TEXT("[BattleState] Granted %d innate immunity tag(s) to %s"),
                            UnitDef->InnateImmunityTags.Num(), *GetNameSafe(Unit));
                    }
                }
            }
        }

        Unit->EnableBattleMode();
        UnitStates.Add(MoveTemp(NewState));

        UE_LOG(LogTemp, Log,
            TEXT("[BattleState] Initialized unit %s | HP=%d | PhysAtk=%d | AP=%d | Move=%d"),
            *Unit->GetName(),
            NewState.MaxHP,
            NewState.PhysicalAttack,
            NewState.MaxActionPoints,
            NewState.MovementRange);
    }

    UE_LOG(LogTemp, Log, TEXT("[BattleState] Initialized with %d units"), UnitStates.Num());
}

void UBattleState::InitializeFromSave(
    UBattlePreparationContext& BattlePrepContext,
    const FSavedCombatSession& SavedSession
)
{
    UnitStates.Empty();

    // Build a lookup map from SaveableComponent FGuid → FSavedCombatUnitState
    // so we can match spawned actors to their saved data in O(1).
    TMap<FGuid, const FSavedCombatUnitState*> SavedByGuid;
    for (const FSavedCombatUnitState& Saved : SavedSession.UnitStates)
    {
        if (Saved.ActorGuid.IsValid())
        {
            SavedByGuid.Add(Saved.ActorGuid, &Saved);
        }
    }

    for (const FSpawnedBattleUnit& Spawned : BattlePrepContext.SpawnedUnits)
    {
        if (!Spawned.UnitActor.IsValid()) { continue; }

        ACharacterBase* Unit = Spawned.UnitActor.Get();
        UUnitStatsComponent* Stats = Unit->GetUnitStats();

        if (Stats)
        {
            Stats->InitializeForBattle();
            // TemporaryAttributeBonuses were already restored by UnitStatsComponent::ReadSaveData.
            // Do NOT reset them here — that would discard the restored bonuses.
        }

        FBattleUnitState NewState;
        NewState.UnitActor = Unit;
        NewState.Team      = Spawned.Team;

        // Find the saved record for this unit via its SaveableComponent guid.
        const FSavedCombatUnitState* Saved = nullptr;
        if (USaveableComponent* SaveComp = Unit->FindComponentByClass<USaveableComponent>())
        {
            Saved = SavedByGuid.FindRef(SaveComp->GetOrCreateGuid());
        }

        if (Stats)
        {
            // RecalculateBattleStats uses the already-restored TemporaryAttributeBonuses,
            // so derived stats (PhysicalAttack, MovementRange, etc.) are correct.
            Stats->RecalculateBattleStats(NewState);
            NewState.MaxHP = Stats->GetMaxHP();
        }

        if (Saved)
        {
            // Restore the volatile combat values — not recalculable from attributes.
            NewState.CurrentHP           = Saved->CurrentHP;
            NewState.CurrentActionPoints = Saved->CurrentActionPoints;

            // Sync UnitStatsComponent::PersistentHP with the saved combat HP so that
            // CombatEventBus::NotifyDamageApplied's IsAlive() guard reads the correct value.
            // Without this, the UnitStatsComponent may hold a stale HP from the world save
            // (e.g. 0 if the unit died in a previous session), causing IsAlive() to return
            // false and OnUnitDied to never broadcast when the unit is killed mid-combat.
            if (Stats)
            {
                Stats->SetCurrentHP(Saved->CurrentHP);
            }

            // Restore status effects WITHOUT calling ApplyPassiveEffect.
            // All stat changes are already baked into the restored TemporaryAttributeBonuses
            // and the FBattleUnitState fields above. We only need the list to be intact
            // so tick timing, expiry, and revert on removal work correctly.
            for (const FSavedActiveStatusEffect& SavedEffect : Saved->ActiveStatusEffects)
            {
                UStatusEffectDefinition* Def = Cast<UStatusEffectDefinition>(
                    SavedEffect.DefinitionPath.TryLoad()
                );
                if (!Def)
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("[BattleState] InitializeFromSave: could not load StatusEffectDefinition at '%s' — skipped"),
                        *SavedEffect.DefinitionPath.ToString());
                    continue;
                }

                FActiveStatusEffect Restored;
                Restored.Definition                = Def;
                Restored.CasterTeam                = SavedEffect.CasterTeam;
                Restored.TurnsRemaining            = SavedEffect.TurnsRemaining;
                Restored.ShieldHP                  = SavedEffect.ShieldHP;
                Restored.CachedCasterStatValue     = SavedEffect.CachedCasterStatValue;
                Restored.CachedMovementRangeSnapshot = SavedEffect.CachedMovementRangeSnapshot;
                Restored.CachedStatDelta           = SavedEffect.CachedStatDelta;
                Restored.InstanceId                = SavedEffect.InstanceId;
                // Caster actor resolution: look up by guid if the actor still exists.
                // Left null if caster is dead — tick formulas handle null caster gracefully.

                NewState.ActiveStatusEffects.Add(MoveTemp(Restored));

                // Re-grant the ASC gameplay tag for this status so that any system
                // querying HasMatchingGameplayTag (e.g. immunity checks, stun checks)
                // sees the correct state after a mid-combat reload.
                // We grant only once per unique tag even if multiple instances exist,
                // mirroring the logic in StatusEffectManager::ApplyStatusEffect.
                const FGameplayTag& StatusTag = Def->StatusTag;
                if (StatusTag.IsValid())
                {
                    if (UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent())
                    {
                        // Check whether any earlier iteration already granted this tag
                        // for this unit to avoid stacking the loose tag count.
                        const bool bTagAlreadyGranted = NewState.ActiveStatusEffects.ContainsByPredicate(
                            [&StatusTag, &Restored](const FActiveStatusEffect& E)
                            {
                                // Compare against all entries except the one we just added.
                                return E.InstanceId != Restored.InstanceId
                                    && E.Definition
                                    && E.Definition->StatusTag == StatusTag;
                            }
                        );

                        if (!bTagAlreadyGranted)
                        {
                            ASC->AddLooseGameplayTags(FGameplayTagContainer(StatusTag));
                            UE_LOG(LogTemp, Log,
                                TEXT("[BattleState] InitializeFromSave: re-granted ASC tag '%s' to %s"),
                                *StatusTag.ToString(), *GetNameSafe(Unit));
                        }
                    }
                }
            }

            UE_LOG(LogTemp, Log,
                TEXT("[BattleState] Restored unit %s | HP=%d/%d | AP=%d | StatusEffects=%d"),
                *GetNameSafe(Unit),
                NewState.CurrentHP, NewState.MaxHP,
                NewState.CurrentActionPoints,
                NewState.ActiveStatusEffects.Num());
        }
        else
        {
            // No saved record for this unit (e.g. freshly spawned enemy with no prior state).
            NewState.CurrentHP           = Stats ? Stats->GetCurrentHP()   : 0;
            NewState.CurrentActionPoints = Stats ? Stats->GetActionPoints() : 0;

            UE_LOG(LogTemp, Warning,
                TEXT("[BattleState] InitializeFromSave: no saved state for %s — using fresh values"),
                *GetNameSafe(Unit));
        }

        // Propagate innate immunity tags.
        if (Stats)
        {
            if (const UUnitDefinition* UnitDef = Stats->GetDefinition())
            {
                if (!UnitDef->InnateImmunityTags.IsEmpty())
                {
                    if (UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent())
                    {
                        ASC->AddLooseGameplayTags(UnitDef->InnateImmunityTags);
                    }
                }
            }
        }

        Unit->EnableBattleMode();
        UnitStates.Add(MoveTemp(NewState));
    }

    UE_LOG(LogTemp, Log,
        TEXT("[BattleState] Restored from save with %d units"), UnitStates.Num());
}

FBattleUnitState* UBattleState::FindUnitState(ACharacterBase* Unit)
{
    return UnitStates.FindByPredicate(
        [Unit](const FBattleUnitState& State)
        {
            return State.UnitActor.Get() == Unit;
        });
}

const FBattleUnitState* UBattleState::FindUnitState(ACharacterBase* Unit) const
{
    return UnitStates.FindByPredicate(
        [Unit](const FBattleUnitState& State)
        {
            return State.UnitActor.Get() == Unit;
        });
}

bool UBattleState::CanUnitAct(ACharacterBase* Unit)
{
    if (!Unit) { return false; }

    const FBattleUnitState* State = FindUnitState(Unit);
    if (!State) { return false; }

    const bool bCanAct = State->CanAct();

    UE_LOG(LogTemp, Log,
        TEXT("[BattleState] CanUnitAct(%s): Alive=%s | AP=%d | Acted=%s => %s"),
        *Unit->GetName(),
        State->IsAlive() ? TEXT("YES") : TEXT("NO"),
        State->CurrentActionPoints,
        State->bHasActedThisTurn ? TEXT("YES") : TEXT("NO"),
        bCanAct ? TEXT("CAN ACT") : TEXT("BLOCKED"));

    return bCanAct;
}

bool UBattleState::IsUnitExhausted(ACharacterBase* Unit)
{
    const FBattleUnitState* State = FindUnitState(Unit);
    return !State || !State->CanAct();
}

void UBattleState::MarkUnitAsActed(ACharacterBase* Unit)
{
    FBattleUnitState* State = FindUnitState(Unit);
    if (!State) { return; }
    State->bHasActedThisTurn = true;
}

void UBattleState::MarkUnitMoved(ACharacterBase* Unit)
{
    FBattleUnitState* State = FindUnitState(Unit);
    if (!State) { return; }
    State->bHasMovedThisTurn = true;
}

void UBattleState::ConsumeActionPoints(ACharacterBase* Unit, int32 Amount)
{
    FBattleUnitState* State = FindUnitState(Unit);
    if (!State || State->bIsDead) { return; }

    State->CurrentActionPoints = FMath::Max(0, State->CurrentActionPoints - Amount);

    UE_LOG(LogTemp, Log,
        TEXT("[BattleState] ConsumeActionPoints: %s spent %d AP — %d remaining"),
        *GetNameSafe(Unit), Amount, State->CurrentActionPoints);
}

void UBattleState::ResetTurnStateForTeam(EBattleUnitTeam Team)
{
    for (FBattleUnitState& State : UnitStates)
    {
        if (State.Team != Team || State.bIsDead) { continue; }

        State.CurrentActionPoints = State.MaxActionPoints;
        State.bHasMovedThisTurn   = false;
        State.bHasActedThisTurn   = false;

        UE_LOG(LogTemp, Log,
            TEXT("[BattleState] Turn reset for %s — AP restored to %d"),
            *GetNameSafe(State.UnitActor.Get()), State.CurrentActionPoints);
    }
}

void UBattleState::ApplyDamage(ACharacterBase* Unit, int32 Amount)
{
    FBattleUnitState* State = FindUnitState(Unit);
    if (!State || State->bIsDead) { return; }

    State->CurrentHP = FMath::Max(0, State->CurrentHP - Amount);

    // On death, clear the active status list so tick effects don't fire on a dead unit.
    // ASC tags are intentionally NOT removed here: StatusEffectManager handles proper
    // passive revert and EventBus notification during its own cleanup pass.
    if (State->CurrentHP <= 0)
    {
        State->bIsDead = true;
        State->ActiveStatusEffects.Empty();

        UE_LOG(LogTemp, Log,
            TEXT("[BattleState] %s died — active status effects cleared"),
            *GetNameSafe(Unit));
    }
}

void UBattleState::ApplyHeal(ACharacterBase* Unit, int32 Amount)
{
    FBattleUnitState* State = FindUnitState(Unit);
    if (!State || State->bIsDead) { return; }

    State->CurrentHP = FMath::Min(State->CurrentHP + Amount, State->MaxHP);
}

void UBattleState::PersistToCombatants()
{
    UE_LOG(LogTemp, Log, TEXT("[BattleState] Persisting battle results to %d units"), UnitStates.Num());

    for (const FBattleUnitState& State : UnitStates)
    {
        ACharacterBase* Unit = State.UnitActor.Get();
        if (!Unit) { continue; }

        UUnitStatsComponent* Stats = Unit->GetUnitStats();
        if (!Stats) { continue; }

        // Write HP back — dead units persist as 0, survivors keep their remaining HP.
        Stats->SetCurrentHP(State.CurrentHP);

        UE_LOG(LogTemp, Log,
            TEXT("[BattleState] Persisted %s — HP: %d / %d | Dead: %s"),
            *GetNameSafe(Unit),
            State.CurrentHP,
            State.MaxHP,
            State.bIsDead ? TEXT("YES") : TEXT("NO"));
    }
}

void UBattleState::GenerateResult()
{
    PersistToCombatants();
}