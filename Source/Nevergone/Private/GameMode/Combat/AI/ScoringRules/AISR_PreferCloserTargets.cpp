// Copyright Xyzto Works


#include "GameMode/Combat/AI/ScoringRules/AISR_PreferCloserTargets.h"

#include "Characters/CharacterBase.h"
#include "Level/GridManager.h"
#include "Types/AIBattleTypes.h"


float UAISR_PreferCloserTargets::ComputeRawScore(
	const FTeamCandidateAction& Candidate,
	const FAICandidateEvalContext& Context) const
{
	if (Candidate.ActionType != ETeamAIActionType::UseAbility)
	{
		return 0.f;
	}

	const ACharacterBase* Target = Candidate.Targeting.TargetActor;
	if (!Target || !Context.Grid || !Context.ActingUnit)
	{
		return 0.f;
	}

	FIntPoint EvalCoord = Context.Grid->WorldToGrid(Context.ActingUnit->GetActorLocation());

	if (Candidate.Targeting.bHasTileTarget)
	{
		EvalCoord = Candidate.Targeting.TargetTile;
	}

	const FIntPoint TargetCoord = Context.Grid->WorldToGrid(Target->GetActorLocation());

	const int32 Dist = FMath::Max(
		FMath::Abs(EvalCoord.X - TargetCoord.X),
		FMath::Abs(EvalCoord.Y - TargetCoord.Y));

	// Convert proximity into a positive score
	// Adjacent = best, far = low.
	const float ProximityScore = FMath::GetMappedRangeValueClamped(
		FVector2D(1.f, 8.f),
		FVector2D(1.f, 0.f),
		static_cast<float>(Dist));

	return ProximityScore;
}
