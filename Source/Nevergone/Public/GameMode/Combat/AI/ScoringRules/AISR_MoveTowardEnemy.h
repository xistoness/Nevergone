// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameMode/Combat/AI/BattleAIScoringRule.h"
#include "AISR_MoveTowardEnemy.generated.h"

/**
 * 
 */
UCLASS()
class NEVERGONE_API UAISR_MoveTowardEnemy : public UBattleAIScoringRule
{
	GENERATED_BODY()
	
public:
	UAISR_MoveTowardEnemy();

	virtual float ComputeRawScore(
		const FTeamCandidateAction& Candidate,
		const FAICandidateEvalContext& Context) const override;
};
