// Copyright Xyzto Works


#include "ActorComponents/UnitStatsComponent.h"

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

	if (!IsAlive()) return;

	// Example: passive regen
	if (PersistentHP < GetMaxHP())
	{
		const float RegenRate = 1.f; // later from definition / buffs
		SetCurrentHP(PersistentHP + RegenRate * DeltaTime);
	}
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

void UUnitStatsComponent::SetCurrentHP(float NewHP)
{
	PersistentHP = FMath::Clamp(NewHP, 0.f, GetMaxHP());
}

void UUnitStatsComponent::SetCurrentActionPoints(int32 ActionPoints)
{
	CurrentActionPoints = ActionPoints;
}

bool UUnitStatsComponent::IsAlive() const
{
	return PersistentHP > 0.f;
}

