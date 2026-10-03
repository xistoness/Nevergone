// Copyright Xyzto Works


#include "Data/ExplorationEnemyDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "ExplorationEnemyDefinition"

EDataValidationResult UExplorationEnemyDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (LoseSightRadius < SightRadius)
	{
		Context.AddError(LOCTEXT("LoseSightTooSmall",
			"LoseSightRadius must be greater than or equal to SightRadius, otherwise the enemy flickers between Chase and Patrol."));

		Result = EDataValidationResult::Invalid;
	}

	if (ChaseSpeed < PatrolSpeed)
	{
		Context.AddWarning(LOCTEXT("ChaseSlowerThanPatrol",
			"ChaseSpeed is lower than PatrolSpeed. The enemy will slow down when it spots the survivor."));
	}

	if (TrackingGraceSeconds >= MemorySeconds)
	{
		Context.AddWarning(LOCTEXT("GraceExceedsMemory",
			"TrackingGraceSeconds is not shorter than MemorySeconds. The enemy will give up the chase while it still knows where the survivor is."));
	}
	
	for (int32 Index = 0; Index < VisualVariants.Num(); ++Index)
	{
		if (!VisualVariants[Index].IsUsable())
		{
			Context.AddError(FText::Format(
				LOCTEXT("EmptyVariant", "VisualVariants[{0}] has no mesh assigned."),
				FText::AsNumber(Index)));

			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif