// Copyright Xyzto Works

#include "GameMode/Combat/BattleState.h"

#include "AbilitySystemComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Data/UnitDefinition.h"
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
        }

        FBattleUnitState NewState;
        NewState.UnitActor = Unit;
        NewState.Team      = Spawned.Team;

        if (Stats)
        {
            NewState.MaxHP               = Stats->GetMaxHP();
            NewState.CurrentHP           = Stats->GetCurrentHP();
            NewState.PhysicalAttack      = Stats->GetPhysicalAttack();
            NewState.RangedAttack        = Stats->GetRangedAttack();
            NewState.MagicalPower        = Stats->GetMagicalPower();
            NewState.PhysicalDefense     = Stats->GetPhysicalDefense();
            NewState.MagicalDefense      = Stats->GetMagicalDefense();
            NewState.MaxActionPoints     = Stats->GetActionPoints();
            NewState.CurrentActionPoints = Stats->GetActionPoints();
            NewState.MovementRange       = Stats->GetMovementRange();
            NewState.HitChanceModifier   = Stats->GetHitChanceModifier();
            NewState.EvasionModifier     = Stats->GetEvasionModifier();
            NewState.CritChance          = Stats->GetCritChance();
            NewState.TraversalParams     = Stats->GetTraversalParams();
        }

        // Propagate innate immunity tags from UnitDefinition to the ASC so
        // StatusEffectManager::IsImmune() can read them via HasMatchingGameplayTag.
        // Equipment and terrain can add more immunity tags at runtime the same way.
        if (Stats)
        {
            if (const UUnitDefinition* UnitDef = Stats->GetDefinition())
            {
                if (!UnitDef->InnateImmunityTags.IsEmpty())
                {
                    if (UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent())
                    {
                        ASC->AddLooseGameplayTags(UnitDef->InnateImmunityTags);

                        UE_LOG(LogNevergone, Log,
                            TEXT("[BattleState] Granted %d innate immunity tag(s) to %s"),
                            UnitDef->InnateImmunityTags.Num(), *GetNameSafe(Unit));
                    }
                }
            }
        }

        Unit->EnableBattleMode();
        UnitStates.Add(MoveTemp(NewState));

        UE_LOG(LogNevergone, Log,
            TEXT("[BattleState] Initialized unit %s | HP=%d | PhysAtk=%d | AP=%d | Move=%d"),
            *Unit->GetName(),
            NewState.MaxHP,
            NewState.PhysicalAttack,
            NewState.MaxActionPoints,
            NewState.MovementRange);
    }

    UE_LOG(LogNevergone, Log, TEXT("[BattleState] Initialized with %d units"), UnitStates.Num());
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

    UE_LOG(LogNevergone, Log,
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

    UE_LOG(LogNevergone, Log,
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

        UE_LOG(LogNevergone, Log,
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

        UE_LOG(LogNevergone, Log,
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
    UE_LOG(LogNevergone, Log, TEXT("[BattleState] Persisting battle results to %d units"), UnitStates.Num());

    for (const FBattleUnitState& State : UnitStates)
    {
        ACharacterBase* Unit = State.UnitActor.Get();
        if (!Unit) { continue; }

        UUnitStatsComponent* Stats = Unit->GetUnitStats();
        if (!Stats) { continue; }

        // Write HP back — dead units persist as 0, survivors keep their remaining HP.
        Stats->SetCurrentHP(State.CurrentHP);

        UE_LOG(LogNevergone, Log,
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