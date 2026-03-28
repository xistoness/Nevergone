// Copyright Xyzto Works


#include "GameMode/Combat/AI/ScoringRules/AISR_ExpectedDamage.h"
#include "Types/AIBattleTypes.h"


UAISR_ExpectedDamage::UAISR_ExpectedDamage()
{
	Weight = 40.f;
}

float UAISR_ExpectedDamage::ComputeRawScore(
	const FTeamCandidateAction& Candidate,
	const FAICandidateEvalContext& Context) const
{
	if (Candidate.ActionType != ETeamAIActionType::UseAbility)
	{
		return 0.f;
	}

	if (Candidate.ExpectedDamage <= 0.f)
	{
		return 0.f;
	}

	const float NormalizedDamage = FMath::GetMappedRangeValueClamped(
		FVector2D(MinDamageForScore, MaxDamageForFullScore),
		FVector2D(0.f, MaxRawScore),
		Candidate.ExpectedDamage);

	return NormalizedDamage;
}
