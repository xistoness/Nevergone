// Copyright Xyzto Works


#include "GameMode/Combat/BattleState.h"

#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "GameMode/BattlePreparationContext.h"

static const FGameplayTag Tag_Incapacitated =
	FGameplayTag::RequestGameplayTag(TEXT("Status.Incapacitated"));

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
		NewState.Team = Spawned.Team;

		// Pull initial stats from character

		NewState.MaxHP     = Stats->GetMaxHP();
		NewState.CurrentHP = Stats->GetCurrentHP();
		NewState.MaxSpeed  = Stats->GetSpeed();
		NewState.MaxMeleeAttack  = Stats->GetMeleeAttack();
		NewState.MaxRangedAttack  = Stats->GetRangedAttack();
		NewState.ActionPoints = Stats->GetActionPoints();

		// Optional: tags from generated data
		if (Spawned.Team == EBattleUnitTeam::Ally &&
			BattlePrepContext.PlayerParty.IsValidIndex(Spawned.SourceIndex))
		{
			NewState.StatusTags.AppendTags(
				BattlePrepContext.PlayerParty[Spawned.SourceIndex].Tags);
		}
		else if (Spawned.Team == EBattleUnitTeam::Enemy &&
			BattlePrepContext.EnemyParty.IsValidIndex(Spawned.SourceIndex))
		{
			NewState.StatusTags.AppendTags(
				BattlePrepContext.EnemyParty[Spawned.SourceIndex].Tags);
		}
		
		Unit->EnableBattleMode();

		UnitStates.Add(MoveTemp(NewState));
	}
	UE_LOG(LogTemp, Warning,
	TEXT("[BattleState] Initialized with %d units"),
	UnitStates.Num()
);

	for (const FBattleUnitState& State : UnitStates)
	{
		if (State.UnitActor.IsValid())
		{
			UE_LOG(LogTemp, Warning,
				TEXT(" - UnitState: %s | HP=%.1f"),
				*State.UnitActor->GetName(),
				State.CurrentHP
			);
		}
	}
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

bool UBattleState::CanUnitAct(ACharacterBase* Unit) const
{

	if (!Unit)
	{

		return false;
	}

	const FBattleUnitState* State = FindUnitState(Unit);
	if (!State)
	{
		return false;
	}

	const bool bIsAlive = State->IsAlive();
	const bool bHasActed = State->bHasActedThisTurn;
	const bool bIsIncapacitated = State->StatusTags.HasTagExact(Tag_Incapacitated);
	const bool bHasActionPoints = 0 < Unit->GetUnitStats()->GetCurrentActionPoints();

	const bool bCanAct = bIsAlive && !bHasActed && !bIsIncapacitated && bHasActionPoints;

	UE_LOG(LogTemp, Warning,
		TEXT("[BattleState] CanUnitAct(%s): Alive=%s | HasActed=%s | Incapacitated=%s => %s"),
		*Unit->GetName(),
		bIsAlive ? TEXT("YES") : TEXT("NO"),
		bHasActed ? TEXT("YES") : TEXT("NO"),
		bIsIncapacitated ? TEXT("YES") : TEXT("NO"),
		bCanAct ? TEXT("CAN ACT") : TEXT("BLOCKED")
	);

	return bCanAct;
}

bool UBattleState::IsUnitExhausted(ACharacterBase* Unit) const
{
	const FBattleUnitState* State = FindUnitState(Unit);
	return !State || !State->CanAct();
}

void UBattleState::MarkUnitAsActed(ACharacterBase* Unit)
{
	FBattleUnitState* State = FindUnitState(Unit);
	if (!State)
	{
		return;
	}

	State->bHasActedThisTurn = true;
}

void UBattleState::MarkUnitMoved(ACharacterBase* Unit)
{
	FBattleUnitState* State = FindUnitState(Unit);
	if (!State)
	{
		return;
	}

	State->bHasMovedThisTurn = true;
}

void UBattleState::ConsumeActionPoints(ACharacterBase* Unit, int32 Amount)
{
	FBattleUnitState* State = FindUnitState(Unit);
	if (!State || State->bIsDead)
	{
		return;
	}

	State->ActionPoints = FMath::Max(0, State->ActionPoints - Amount);
}

void UBattleState::ResetTurnStateForTeam(EBattleUnitTeam Team)
{
	for (FBattleUnitState& State : UnitStates)
	{
		if (State.Team != Team || State.bIsDead)
		{
			continue;
		}

		State.ActionPoints = 2; // vindo de stats no futuro
		State.bHasMovedThisTurn = false;
	}
}

void UBattleState::ApplyDamage(ACharacterBase* Unit, float Amount)
{
	FBattleUnitState* State = FindUnitState(Unit);
	if (!State || State->bIsDead)
	{
		return;
	}

	State->CurrentHP = FMath::Max(0.0f, State->CurrentHP - Amount);

	if (State->CurrentHP <= 0.0f)
	{
		State->bIsDead = true;
		State->StatusTags.Reset();
	}
}

void UBattleState::ApplyStatusTag(ACharacterBase* Unit, const FGameplayTag& StatusTag)
{
	FBattleUnitState* State = FindUnitState(Unit);
	if (!State || State->bIsDead)
	{
		return;
	}

	State->StatusTags.AddTag(StatusTag);
}

void UBattleState::ClearStatusTag(ACharacterBase* Unit, const FGameplayTag& StatusTag)
{
	FBattleUnitState* State = FindUnitState(Unit);
	if (!State)
	{
		return;
	}

	State->StatusTags.RemoveTag(StatusTag);
}

void UBattleState::GenerateResult()
{
	// Convert UnitStates into a battle result:
	// - Dead units
	// - Remaining HP
	// - Persistent relationship changes
	// - Flags for PartyManagerSubsystem
}
