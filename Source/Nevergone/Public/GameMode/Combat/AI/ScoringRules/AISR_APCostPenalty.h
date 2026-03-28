// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameMode/Combat/AI/BattleAIScoringRule.h"
#include "AISR_APCostPenalty.generated.h"

/**
 * 
 */
UCLASS()
class NEVERGONE_API UAISR_APCostPenalty : public UBattleAIScoringRule
{
	GENERATED_BODY()

public:
	UAISR_APCostPenalty();

protected:
	UPROPERTY(EditAnywhere, Category="AI")
	float MinAPCost = 0.f;

	UPROPERTY(EditAnywhere, Category="AI")
	float MaxAPCostForFullPenalty = 4.f;

	UPROPERTY(EditAnywhere, Category="AI")
	float MaxRawPenalty = -1.f;

public:
	virtual float ComputeRawScore(
		const FTeamCandidateAction& Candidate,
		const FAICandidateEvalContext& Context) const override;
	
};
