// Copyright Xyzto Works


#include "GameMode/Combat/AI/ScoringRules/AISR_APCostPenalty.h"

#include "Types/AIBattleTypes.h"

UAISR_APCostPenalty::UAISR_APCostPenalty()
{
	Weight = 6.f;
}

float UAISR_APCostPenalty::ComputeRawScore(
	const FTeamCandidateAction& Candidate,
	const FAICandidateEvalContext& Context) const
{
	if (Candidate.ActionType != ETeamAIActionType::UseAbility)
	{
		return 0.f;
	}

	const float PenaltyMagnitude = FMath::GetMappedRangeValueClamped(
		FVector2D(MinAPCost, MaxAPCostForFullPenalty),
		FVector2D(0.f, 1.f),
		Candidate.ExpectedAPCost);

	return -PenaltyMagnitude;
}
