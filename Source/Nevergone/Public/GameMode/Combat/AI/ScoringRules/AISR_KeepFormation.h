// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameMode/Combat/AI/BattleAIScoringRule.h"
#include "AISR_KeepFormation.generated.h"

/**
 * 
 */
UCLASS()
class NEVERGONE_API UAISR_KeepFormation : public UBattleAIScoringRule
{
	GENERATED_BODY()
	
public:
	virtual float ComputeRawScore(
		const FTeamCandidateAction& Candidate,
		const FAICandidateEvalContext& Context) const override;
	
};
