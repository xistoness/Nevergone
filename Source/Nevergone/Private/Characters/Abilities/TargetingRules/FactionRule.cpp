// Copyright Xyzto Works

#include "Characters/Abilities/TargetingRules/FactionRule.h"

#include "ActorComponents/BattleModeComponent.h"
#include "Characters/CharacterBase.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/BattleUnitState.h"

bool FactionRule::IsSatisfied(const FActionContext& Context) const
{
	ACharacterBase* Caster = Cast<ACharacterBase>(Context.SourceActor);
	ACharacterBase* TargetUnit = Cast<ACharacterBase>(Context.TargetActor);

	if (!Caster || !TargetUnit)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactionRule] Invalid Caster or TargetUnit"));
		return false;
	}

	if (Type == EFactionType::Self)
	{
		return Caster == TargetUnit;
	}

	// Use BattleUnitState::Team as the authoritative source for faction during combat.
	// UnitStatsComponent::AllyTeam/EnemyTeam is a static field not kept in sync at runtime.
	const UBattleModeComponent* BattleMode = Caster->GetBattleModeComponent();
	const UBattleState* BattleStateRef = BattleMode ? BattleMode->GetBattleState() : nullptr;

	if (!BattleStateRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactionRule] BattleState not found on caster %s — faction check failed"),
			*GetNameSafe(Caster));
		return false;
	}

	const FBattleUnitState* CasterState = BattleStateRef->FindUnitState(Caster);
	const FBattleUnitState* TargetState = BattleStateRef->FindUnitState(TargetUnit);

	if (!CasterState || !TargetState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactionRule] BattleUnitState not found for caster or target"));
		return false;
	}

	switch (Type)
	{
	case EFactionType::Ally:
		return CasterState->Team == TargetState->Team;

	case EFactionType::Enemy:
		return CasterState->Team != TargetState->Team;

	default:
		UE_LOG(LogTemp, Warning, TEXT("[FactionRule] Unhandled EFactionType"));
		return false;
	}
}