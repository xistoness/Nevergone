// Copyright Xyzto Works

#include "Characters/Abilities/TargetingRules/MoveRangeRule.h"

#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "GameMode/Combat/BattleState.h"
#include "Types/BattleTypes.h"

bool MoveRangeRule::IsSatisfied(const FActionContext& Context) const
{
	ACharacterBase* Character = Cast<ACharacterBase>(Context.SourceActor);
	if (!Character) { return false; }

	if (Context.CachedPathCost == INDEX_NONE) { return false; }

	UBattleModeComponent* BattleMode    = Character->GetBattleModeComponent();
	UBattleState*         BattleStateRef = BattleMode ? BattleMode->GetBattleState() : nullptr;

	if (!BattleStateRef)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MoveRangeRule] BattleState not available for %s — move denied"),
			*GetNameSafe(Character));
		return false;
	}

	const FBattleUnitState* UnitState = BattleStateRef->FindUnitState(Character);
	if (!UnitState) { return false; }

	const int32 MaxRange = UnitState->MovementRange * UnitState->CurrentActionPoints;
	return Context.CachedPathCost <= MaxRange;
}
