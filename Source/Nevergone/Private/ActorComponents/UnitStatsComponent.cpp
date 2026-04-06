// Copyright Xyzto Works

#include "ActorComponents/UnitStatsComponent.h"

#include "Characters/CharacterBase.h"
#include "Data/ActorSaveData.h"
#include "Data/UnitDefinition.h"
#include "GameInstance/SaveKeys.h"
#include "GameMode/Combat/BattleUnitState.h"

UUnitStatsComponent::UUnitStatsComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UUnitStatsComponent::InitializeForBattle()
{
    // Ensure HP is initialized on first spawn
    if (PersistentHP <= 0.f && Definition)
    {
        PersistentHP = GetMaxHP();
    }
}

void UUnitStatsComponent::ResetTemporaryBonuses()
{
    TemporaryAttributeBonuses = FUnitAttributes{};

    UE_LOG(LogTemp, Log,
        TEXT("[UnitStatsComponent] TemporaryAttributeBonuses reset for %s"),
        *GetNameSafe(GetOwner()));
}

void UUnitStatsComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UUnitStatsComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Passive regen and time-based effects must go through CombatEventBus.
    // Do not mutate HP here.
}

// ---------------------------------------------------------------------------
// Save / Load
// ---------------------------------------------------------------------------

void UUnitStatsComponent::WriteSaveData_Implementation(FActorSaveData& OutData) const
{
    FSavePayload Payload;
    FMemoryWriter Writer(Payload.Data, true);
    FArchive& Ar = Writer;

    // Persist level, HP, base attributes, and temporary bonuses.
    // Temporary bonuses are only non-zero during an active combat session,
    // but we always write them so mid-combat saves restore correctly.
    int32 TempLevel = Level;
    int32 TempHP = PersistentHP;
    FUnitAttributes TempAttrs = Attributes;
    FUnitAttributes TempBonuses = TemporaryAttributeBonuses;

    Ar << TempLevel;
    Ar << TempHP;
    Ar << TempAttrs.Constitution;
    Ar << TempAttrs.Strength;
    Ar << TempAttrs.Dexterity;
    Ar << TempAttrs.Knowledge;
    Ar << TempAttrs.Focus;
    Ar << TempAttrs.Technique;
    Ar << TempAttrs.Evasiveness;
    Ar << TempAttrs.Speed;
    Ar << TempBonuses.Constitution;
    Ar << TempBonuses.Strength;
    Ar << TempBonuses.Dexterity;
    Ar << TempBonuses.Knowledge;
    Ar << TempBonuses.Focus;
    Ar << TempBonuses.Technique;
    Ar << TempBonuses.Evasiveness;
    Ar << TempBonuses.Speed;

    OutData.CustomDataMap.Add(SaveKeys::UnitStats, MoveTemp(Payload));
}

void UUnitStatsComponent::ReadSaveData_Implementation(const FActorSaveData& InData)
{
    const FSavePayload* Payload = InData.CustomDataMap.Find(SaveKeys::UnitStats);
    if (!Payload)
    {
        return;
    }

    FMemoryReader Reader(Payload->Data, true);
    FArchive& Ar = Reader;

    Ar << Level;
    Ar << PersistentHP;
    Ar << Attributes.Constitution;
    Ar << Attributes.Strength;
    Ar << Attributes.Dexterity;
    Ar << Attributes.Knowledge;
    Ar << Attributes.Focus;
    Ar << Attributes.Technique;
    Ar << Attributes.Evasiveness;
    Ar << Attributes.Speed;

    // Temporary bonuses — only present in saves written mid-combat.
    // The reader is positioned right after base attributes; if the payload
    // is shorter (older save format), the archive simply won't read past EOF
    // and bonuses stay at zero — safe fallback.
    Ar << TemporaryAttributeBonuses.Constitution;
    Ar << TemporaryAttributeBonuses.Strength;
    Ar << TemporaryAttributeBonuses.Dexterity;
    Ar << TemporaryAttributeBonuses.Knowledge;
    Ar << TemporaryAttributeBonuses.Focus;
    Ar << TemporaryAttributeBonuses.Technique;
    Ar << TemporaryAttributeBonuses.Evasiveness;
    Ar << TemporaryAttributeBonuses.Speed;
}

// ---------------------------------------------------------------------------
// Effective attribute helpers — always use these inside stat getters.
// Base + temporary bonuses = what abilities and checks should see.
// ---------------------------------------------------------------------------

static FORCEINLINE int32 Eff(int32 Base, int32 Bonus) { return Base + Bonus; }

// ---------------------------------------------------------------------------
// Concrete stat getters — use effective attributes (base + temporary bonuses).
// Call RecalculateBattleStats() to push changes into FBattleUnitState.
// ---------------------------------------------------------------------------

int32 UUnitStatsComponent::GetMaxHP() const
{
    if (!Definition) return 0;
    return FMath::RoundToInt(
        (Definition->HPPerLevel * Level)
        + (Eff(Attributes.Constitution, TemporaryAttributeBonuses.Constitution) * 5.f));
}

int32 UUnitStatsComponent::GetPhysicalAttack() const
{
    if (!Definition) return 0;
    return FMath::RoundToInt(
        (Definition->PhysAttkPerLevel * Level)
        + (Eff(Attributes.Strength, TemporaryAttributeBonuses.Strength) * 1.5f));
}

int32 UUnitStatsComponent::GetRangedAttack() const
{
    if (!Definition) return 0;
    return FMath::RoundToInt(
        (Definition->RangedAttkPerLevel * Level)
        + (Eff(Attributes.Dexterity, TemporaryAttributeBonuses.Dexterity) * 1.5f));
}

int32 UUnitStatsComponent::GetMagicalPower() const
{
    if (!Definition) return 0;
    return FMath::RoundToInt(
        (Definition->MagPwrPerLevel * Level)
        + (Eff(Attributes.Knowledge, TemporaryAttributeBonuses.Knowledge) * 1.5f));
}

int32 UUnitStatsComponent::GetPhysicalDefense() const
{
    if (!Definition) return 0;
    return FMath::RoundToInt(
        (Definition->PhysDefPerLevel * Level)
        + (Eff(Attributes.Constitution, TemporaryAttributeBonuses.Constitution) * 1.5f));
}

int32 UUnitStatsComponent::GetMagicalDefense() const
{
    if (!Definition) return 0;
    return FMath::RoundToInt(
        (Definition->MagDefPerLevel * Level)
        + (Eff(Attributes.Focus, TemporaryAttributeBonuses.Focus) / 5.f));
}

int32 UUnitStatsComponent::GetActionPoints() const
{
    if (!Definition) return 1;
    return Definition->BaseActionPoints
        + (Eff(Attributes.Technique, TemporaryAttributeBonuses.Technique) / 5);
}

int32 UUnitStatsComponent::GetMovementRange() const
{
    if (!Definition) return 1;
    return Definition->BaseMoveRange
        + Eff(Attributes.Speed, TemporaryAttributeBonuses.Speed);
}

int32 UUnitStatsComponent::GetHitChanceModifier() const
{
    return Eff(Attributes.Focus,      TemporaryAttributeBonuses.Focus)
         + Eff(Attributes.Technique,  TemporaryAttributeBonuses.Technique);
}

int32 UUnitStatsComponent::GetEvasionModifier() const
{
    return Eff(Attributes.Focus,        TemporaryAttributeBonuses.Focus)
         + Eff(Attributes.Evasiveness,  TemporaryAttributeBonuses.Evasiveness);
}

int32 UUnitStatsComponent::GetCritChance() const
{
    return Eff(Attributes.Technique, TemporaryAttributeBonuses.Technique) * 2;
}

void UUnitStatsComponent::RecalculateBattleStats(FBattleUnitState& OutState) const
{
    // Repopulates all derived fields from the current effective attributes.
    // Does NOT touch CurrentHP, CurrentActionPoints, ActiveStatusEffects,
    // or turn flags — those are managed by BattleState and StatusEffectManager.
    OutState.MaxHP             = GetMaxHP();
    OutState.PhysicalAttack    = GetPhysicalAttack();
    OutState.RangedAttack      = GetRangedAttack();
    OutState.MagicalPower      = GetMagicalPower();
    OutState.PhysicalDefense   = GetPhysicalDefense();
    OutState.MagicalDefense    = GetMagicalDefense();
    OutState.MaxActionPoints   = GetActionPoints();
    OutState.MovementRange     = GetMovementRange();
    OutState.HitChanceModifier = GetHitChanceModifier();
    OutState.EvasionModifier   = GetEvasionModifier();
    OutState.CritChance        = GetCritChance();
    OutState.TraversalParams   = GetTraversalParams();

    UE_LOG(LogTemp, Verbose,
        TEXT("[UnitStatsComponent] RecalculateBattleStats for %s — PhysAtk=%d RngAtk=%d MagPwr=%d PhysDef=%d MagDef=%d MaxAP=%d Move=%d"),
        *GetNameSafe(GetOwner()),
        OutState.PhysicalAttack, OutState.RangedAttack, OutState.MagicalPower,
        OutState.PhysicalDefense, OutState.MagicalDefense,
        OutState.MaxActionPoints, OutState.MovementRange);
}

// ---------------------------------------------------------------------------
// Persistent getters
// ---------------------------------------------------------------------------

int32 UUnitStatsComponent::GetAllyTeam() const { return AllyTeam; }
int32 UUnitStatsComponent::GetEnemyTeam() const { return EnemyTeam; }
void UUnitStatsComponent::SetAllyTeam(int32 Team) { AllyTeam = Team; }
void UUnitStatsComponent::SetEnemyTeam(int32 Team) { EnemyTeam = Team; }

int32 UUnitStatsComponent::GetCurrentHP() const { return PersistentHP; }

FGridTraversalParams UUnitStatsComponent::GetTraversalParams() const
{
    if (!Definition) return FGridTraversalParams{};
    return Definition->TraversalParams;
}

void UUnitStatsComponent::SetCurrentHP(int32 NewHP)
{
    const bool bWasAlive = IsAlive();
    PersistentHP = FMath::Clamp(NewHP, 0, GetMaxHP());

    if (bWasAlive && !IsAlive())
    {
        if (ACharacterBase* Owner = Cast<ACharacterBase>(GetOwner()))
        {
            OnUnitDeath.Broadcast(Owner);
        }
    }
}

void UUnitStatsComponent::SetCurrentActionPoints(int32 ActionPoints)
{
    CurrentActionPoints = ActionPoints;
    UE_LOG(LogTemp, Log, TEXT("[UnitStatsComponent] %s AP set to %d"),
        *GetNameSafe(GetOwner()), CurrentActionPoints);
}

bool UUnitStatsComponent::IsAlive() const
{
    return PersistentHP > 0;
}