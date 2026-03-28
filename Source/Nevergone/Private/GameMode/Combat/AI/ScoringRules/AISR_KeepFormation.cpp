// Copyright Xyzto Works


#include "GameMode/Combat/AI/ScoringRules/AISR_KeepFormation.h"

#include "Characters/CharacterBase.h"
#include "Level/GridManager.h"
#include "Types/AIBattleTypes.h"


float UAISR_KeepFormation::ComputeRawScore(
	const FTeamCandidateAction& Candidate,
	const FAICandidateEvalContext& Context) const
{
	if (!Context.Grid || !Context.ActingUnit)
	{
		return 0.f;
	}

	if (!Candidate.Targeting.bHasTileTarget)
	{
		return 0.f;
	}

	const FIntPoint Destination = Candidate.Targeting.TargetTile;

	float SumDist = 0.f;
	int32 Count = 0;

	for (ACharacterBase* Ally : Context.Allies)
	{
		if (!Ally || Ally == Context.ActingUnit)
		{
			continue;
		}

		const FIntPoint AllyCoord = Context.Grid->WorldToGrid(Ally->GetActorLocation());
		const int32 Dist = FMath::Max(
			FMath::Abs(AllyCoord.X - Destination.X),
			FMath::Abs(AllyCoord.Y - Destination.Y));

		SumDist += static_cast<float>(Dist);
		++Count;
	}

	if (Count == 0)
	{
		return 0.f;
	}

	const float AvgDist = SumDist / static_cast<float>(Count);

	// Lower average distance is better.
	return -AvgDist;
}
