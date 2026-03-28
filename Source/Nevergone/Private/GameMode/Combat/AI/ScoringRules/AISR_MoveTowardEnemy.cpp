// Copyright Xyzto Works


#include "GameMode/Combat/AI/ScoringRules/AISR_MoveTowardEnemy.h"

#include "Characters/CharacterBase.h"
#include "Level/GridManager.h"
#include "Types/AIBattleTypes.h"

UAISR_MoveTowardEnemy::UAISR_MoveTowardEnemy()
{
	Weight = 8.f;
}

float UAISR_MoveTowardEnemy::ComputeRawScore(
	const FTeamCandidateAction& Candidate,
	const FAICandidateEvalContext& Context) const
{
	if (Candidate.ActionType != ETeamAIActionType::Move)
	{
		return 0.f;
	}

	if (!Context.ActingUnit || !Context.Grid || Context.Enemies.Num() == 0)
	{
		return 0.f;
	}

	if (!Candidate.Targeting.bHasTileTarget)
	{
		return 0.f;
	}

	const FIntPoint SourceCoord = Context.Grid->WorldToGrid(Context.ActingUnit->GetActorLocation());
	const FIntPoint DestCoord = Candidate.Targeting.TargetTile;

	int32 BestCurrentDistance = TNumericLimits<int32>::Max();
	int32 BestNewDistance = TNumericLimits<int32>::Max();

	for (ACharacterBase* Enemy : Context.Enemies)
	{
		if (!Enemy)
		{
			continue;
		}

		const FIntPoint EnemyCoord = Context.Grid->WorldToGrid(Enemy->GetActorLocation());

		const int32 CurrentDistance = FMath::Max(
			FMath::Abs(SourceCoord.X - EnemyCoord.X),
			FMath::Abs(SourceCoord.Y - EnemyCoord.Y));

		const int32 NewDistance = FMath::Max(
			FMath::Abs(DestCoord.X - EnemyCoord.X),
			FMath::Abs(DestCoord.Y - EnemyCoord.Y));

		BestCurrentDistance = FMath::Min(BestCurrentDistance, CurrentDistance);
		BestNewDistance = FMath::Min(BestNewDistance, NewDistance);
	}

	if (BestCurrentDistance == TNumericLimits<int32>::Max() ||
		BestNewDistance == TNumericLimits<int32>::Max())
	{
		return 0.f;
	}

	return static_cast<float>(BestCurrentDistance - BestNewDistance);
}
