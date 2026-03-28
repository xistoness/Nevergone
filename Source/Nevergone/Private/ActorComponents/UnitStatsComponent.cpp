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
	// Safety: initialize HP on first spawn
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

	// Passive regen and other time-based combat effects should be driven
	// by the CombatManager or a dedicated StatusEffectComponent so that
	// they can properly go through the CombatEventBus (floating text,
	// BattleState sync, death checks). Do not add HP mutations here.
}

void UUnitStatsComponent::WriteSaveData_Implementation(FActorSaveData& OutData) const
{
	FSavePayload Payload;

	FMemoryWriter Writer(Payload.Data, true);
	FArchive& Ar = Writer;

	int32 TempLevel = Level;
	float TempPersistentHP = PersistentHP;

	Ar << TempLevel;
	Ar << TempPersistentHP;

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
}

float UUnitStatsComponent::GetMaxHP() const
{
	if (!Definition) return 0.f;

	return Definition->BaseHP + (Level - 1) * Definition->HPPerLevel;
}

float UUnitStatsComponent::GetSpeed() const
{
	if (!Definition) return 0.f;

	return Definition->BaseSpeed;
}

float UUnitStatsComponent::GetMeleeAttack() const
{
	if (!Definition) return 0.f;

	return Definition->BaseMeleeAttack;
}

float UUnitStatsComponent::GetRangedAttack() const
{
	if (!Definition) return 0.f;

	return Definition->BaseRangedAttack;
}

float UUnitStatsComponent::GetActionPoints() const
{
	if (!Definition) return 0.f;

	return Definition->BaseActionPoints;
}

float UUnitStatsComponent::GetCurrentActionPoints() const
{
	return CurrentActionPoints;
}

int32 UUnitStatsComponent::GetAllyTeam() const
{
	return AllyTeam;
}

int32 UUnitStatsComponent::GetEnemyTeam() const
{
	return EnemyTeam;
}

void UUnitStatsComponent::SetEnemyTeam(int32 Team)
{
	EnemyTeam = Team;
}

void UUnitStatsComponent::SetAllyTeam(int32 Team)
{
	AllyTeam = Team;
}

float UUnitStatsComponent::GetCurrentHP() const
{
	return PersistentHP;
}

FGridTraversalParams UUnitStatsComponent::GetTraversalParams() const
{
	return Definition->TraversalParams;
}

void UUnitStatsComponent::SetCurrentHP(float NewHP)
{
	const bool bWasAlive = IsAlive();

	PersistentHP = FMath::Clamp(NewHP, 0.f, GetMaxHP());

	// Fire death delegate so CombatManager can react (grid removal,
	// win/lose check). All other consumers (floating text, BattleState)
	// are notified by UCombatEventBus before this setter is called.
	const bool bIsAliveNow = IsAlive();
	if (bWasAlive && !bIsAliveNow)
	{
		if (ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOwner()))
		{
			OnUnitDeath.Broadcast(OwnerCharacter);
		}
	}
}

void UUnitStatsComponent::SetCurrentActionPoints(int32 ActionPoints)
{
	CurrentActionPoints = ActionPoints;
	UE_LOG(LogTemp, Warning, TEXT("[%s]: Current AP: %d"), *GetOwner()->GetName(), CurrentActionPoints);
}

bool UUnitStatsComponent::IsAlive() const
{
	return PersistentHP > 0.f;
}