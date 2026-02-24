#include "Characters/Abilities/TargetingRules/MoveRangeRule.h"

#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Types/BattleTypes.h"

bool MoveRangeRule::IsSatisfied(const FActionContext& Context) const
{
	ACharacterBase* Character = Cast<ACharacterBase>(Context.SourceActor);
	if (!Character) return false;
	
	const int32 Speed = Character->GetUnitStats()->GetSpeed();
	const int32 ActionPoints = Character->GetUnitStats()->GetCurrentActionPoints();
	
	int32 MaxRange = Speed * ActionPoints;
	
	if (Context.CachedPathCost == INDEX_NONE)
		return false;

	return Context.CachedPathCost <= MaxRange;
}
