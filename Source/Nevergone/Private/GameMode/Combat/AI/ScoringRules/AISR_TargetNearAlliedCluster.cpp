// Copyright Xyzto Works


#include "GameMode/Combat/AI/ScoringRules/AISR_TargetNearAlliedCluster.h"

#include "Characters/CharacterBase.h"
#include "Level/GridManager.h"
#include "Types/AIBattleTypes.h"


float UAISR_TargetNearAlliedCluster::ComputeRawScore(
	const FTeamCandidateAction& Candidate,
	const FAICandidateEvalContext& Context) const
{
	const ACharacterBase* Target = Candidate.Targeting.TargetActor;
	if (!Target || !Context.Grid)
	{
		return 0.f;
	}
	
	if (!Candidate.Targeting.bHasActorTarget)
	{
		return 0.f;
	}

	const FIntPoint TargetCoord = Context.Grid->WorldToGrid(Target->GetActorLocation());

	float SumDist = 0.f;
	int32 Count = 0;

	for (ACharacterBase* Ally : Context.Allies)
	{
		if (!Ally)
		{
			continue;
		}

		const FIntPoint AllyCoord = Context.Grid->WorldToGrid(Ally->GetActorLocation());
		const int32 Dist = FMath::Max(
			FMath::Abs(AllyCoord.X - TargetCoord.X),
			FMath::Abs(AllyCoord.Y - TargetCoord.Y));

		SumDist += static_cast<float>(Dist);
		++Count;
	}

	if (Count == 0)
	{
		return 0.f;
	}

	const float AvgDist = SumDist / static_cast<float>(Count);

	// Targets closer to the allied cluster are better.
	return -AvgDist;
}
