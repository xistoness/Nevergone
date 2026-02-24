#include "Characters/Abilities/TargetingRules/TileNotBlockedRule.h"
#include "Types/BattleTypes.h"

bool TileNotBlockedRule::IsSatisfied(const FActionContext& Context) const
{
	return !Context.bIsTileBlocked;
}