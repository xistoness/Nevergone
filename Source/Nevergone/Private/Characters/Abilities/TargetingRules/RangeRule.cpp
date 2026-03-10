#include "Public/Characters/Abilities/TargetingRules/RangeRule.h"
#include "GameFramework/Actor.h"

#include "Characters/CharacterBase.h"

bool RangeRule::IsSatisfied(const FActionContext& Context) const
{
	const FIntPoint& Source = Context.SourceGridCoord;
	const FIntPoint& Target = Context.HoveredGridCoord;

	int32 Distance =
		FMath::Max(
			FMath::Abs(Source.X - Target.X),
			FMath::Abs(Source.Y - Target.Y)
		);

	return Distance <= MaxRange;
}
