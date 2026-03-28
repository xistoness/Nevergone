// Copyright Xyzto Works


#include "GameMode/Combat/AI/BattleAICandidateScorer.h"

#include "GameMode/Combat/AI/BattleAIBehaviorProfile.h"
#include "GameMode/Combat/AI/BattleAIScoringRule.h"

float UBattleAICandidateScorer::ScoreCandidate(const FTeamCandidateAction& Candidate,
                                               const FAICandidateEvalContext& Context) const
{
	float TotalScore = 0.f;

	for (const UBattleAIScoringRule* Rule : Rules)
	{
		if (!IsValid(Rule))
		{
			continue;
		}

		TotalScore += Rule->Evaluate(Candidate, Context);
	}

	return TotalScore;
}

void UBattleAICandidateScorer::SetRules(const UBattleAIBehaviorProfile* Profile)
{
	Rules.Reset();

	if (!Profile)
	{
		return;
	}

	for (UBattleAIScoringRule* Rule : Profile->ScoringRules)
	{
		if (Rule)
		{
			Rules.Add(Rule);
		}
	}
}
