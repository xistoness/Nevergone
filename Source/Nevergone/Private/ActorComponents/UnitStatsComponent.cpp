// Copyright Xyzto Works

#include "ActorComponents/UnitStatsComponent.h"

#include "Characters/CharacterBase.h"
#include "Data/ActorSaveData.h"
#include "Data/UnitDefinition.h"
#include "GameInstance/SaveKeys.h"

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

    // Persist level, HP, and all 8 base attributes
    int32 TempLevel = Level;
    int32 TempHP = PersistentHP;
    FUnitAttributes TempAttrs = Attributes;

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
}

// ---------------------------------------------------------------------------
// Concrete stat getters — use these to populate BattleUnitState at battle start.
// During combat, always read from BattleUnitState instead.
// ---------------------------------------------------------------------------

int32 UUnitStatsComponent::GetMaxHP() const
{
    if (!Definition) return 0;
    return FMath::RoundToInt((Definition->HPPerLevel * Level) + (Attributes.Constitution * 5.f));
}

int32 UUnitStatsComponent::GetPhysicalAttack() const
{
    if (!Definition) return 0;
    return FMath::RoundToInt((Definition->PhysAttkPerLevel * Level) + (Attributes.Strength * 1.5f));
}

int32 UUnitStatsComponent::GetRangedAttack() const
{
    if (!Definition) return 0;
    return FMath::RoundToInt((Definition->RangedAttkPerLevel * Level) + (Attributes.Dexterity * 1.5f));
}

int32 UUnitStatsComponent::GetMagicalPower() const
{
    if (!Definition) return 0;
    return FMath::RoundToInt((Definition->MagPwrPerLevel * Level) + (Attributes.Knowledge * 1.5f));
}

int32 UUnitStatsComponent::GetPhysicalDefense() const
{
    if (!Definition) return 0;
    return FMath::RoundToInt((Definition->PhysDefPerLevel * Level) + (Attributes.Constitution * 1.5f));
}

int32 UUnitStatsComponent::GetMagicalDefense() const
{
    if (!Definition) return 0;
    return FMath::RoundToInt((Definition->MagDefPerLevel * Level) + (Attributes.Focus / 5.f));
}

int32 UUnitStatsComponent::GetActionPoints() const
{
    if (!Definition) return 1;
    return Definition->BaseActionPoints + (Attributes.Technique / 5);
}

int32 UUnitStatsComponent::GetMovementRange() const
{
    if (!Definition) return 1;
    return Definition->BaseMoveRange + Attributes.Speed;
}

int32 UUnitStatsComponent::GetHitChanceModifier() const
{
    return Attributes.Focus + Attributes.Technique;
}

int32 UUnitStatsComponent::GetEvasionModifier() const
{
    return Attributes.Focus + Attributes.Evasiveness;
}

int32 UUnitStatsComponent::GetCritChance() const
{
    return Attributes.Technique * 2;
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