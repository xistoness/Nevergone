// Copyright Xyzto Works


#include "GameMode/Combat/AI/BattleAIScoringRule.h"

float UBattleAIScoringRule::Evaluate(
	const FTeamCandidateAction& Candidate,
	const FAICandidateEvalContext& Context) const
{
	return ComputeRawScore(Candidate, Context) * Weight;
}
