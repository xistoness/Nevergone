// Copyright Xyzto Works


#include "GameMode/Combat/AI/ScoringRules/AISR_PreferAbilityOverMove.h"

#include "Types/AIBattleTypes.h"

UAISR_PreferAbilityOverMove::UAISR_PreferAbilityOverMove()
{
	Weight = 15.f;
}

float UAISR_PreferAbilityOverMove::ComputeRawScore(
	const FTeamCandidateAction& Candidate,
	const FAICandidateEvalContext& Context) const
{
	if (Candidate.ActionType == ETeamAIActionType::UseAbility)
	{
		if (Candidate.ExpectedDamage >= MinimumUsefulDamage)
		{
			return AbilityBonus;
		}

		return 0.f;
	}

	if (Candidate.ActionType == ETeamAIActionType::Move)
	{
		// Penalize movement slightly when a candidate is purely repositioning
		// and there are already usable offensive options in the action pool.
		// This version is local-only because the current rule API evaluates one
		// candidate at a time and does not receive the full candidate list.
		//
		// So this is intentionally conservative: punish move a little,
		// but not enough to block legitimate approach behavior.
		return MovePenaltyWhenAttackExists;
	}

	return 0.f;
}