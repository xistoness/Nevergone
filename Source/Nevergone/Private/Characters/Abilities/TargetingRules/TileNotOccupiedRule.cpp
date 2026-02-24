#include "Characters/Abilities/TargetingRules/TileNotOccupiedRule.h"

bool TileNotOccupiedRule::IsSatisfied(const FActionContext& Context) const
{
	if (!Context.SourceActor)
		return false;

	UWorld* World = Context.SourceActor->GetWorld();
	if (!World)
		return false;

	if (!Context.TargetActor)
		return true;

	return false;
}
