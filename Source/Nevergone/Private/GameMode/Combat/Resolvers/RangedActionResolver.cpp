#pragma once

#include "GameMode/Combat/Resolvers/RangedActionResolver.h"
#include "GameMode/BattlefieldQuerySubsystem.h"
#include "Types/BattleTypes.h"

FActionResult RangedActionResolver::Resolve(const FActionContext& Context, UBattlefieldQuerySubsystem* QuerySubsystem,
	bool bIsPreview)
{
	FActionResult Result;

	// Basic validation
	if (!Context.SourceActor || !Context.TargetActor || !Context.Ability)
	{
		return Result;
	}

	// Range check
	const float Distance = FVector::Distance(
		Context.SourceActor->GetActorLocation(),
		Context.TargetActor->GetActorLocation()
	);

	if (Distance > Context.Ability->MaxRange)
	{
		return Result;
	}

	// Line of effect check
	const bool bHasLine =
		QuerySubsystem->HasLineOfEffect(
			Context.SourceActor->GetActorLocation(),
			Context.TargetActor->GetActorLocation(),
			{ Context.SourceActor }
		);

	if (!bHasLine)
	{
		return Result;
	}

	// Compute hit chance
	Result.HitChance = Context.Ability->BaseHitChance;
	Result.ExpectedDamage = Context.Ability->BaseDamage;
	Result.AffectedActors.Add(Context.TargetActor);
	Result.bIsValid = true;

	// Commit effects only if not preview
	if (!bIsPreview)
	{
		// Apply damage here (delegated elsewhere ideally)
	}

	return Result;	
}
