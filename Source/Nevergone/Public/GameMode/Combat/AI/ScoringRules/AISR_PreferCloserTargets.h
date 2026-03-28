// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameMode/Combat/AI/BattleAIScoringRule.h"
#include "AISR_PreferCloserTargets.generated.h"

/**
 * 
 */
UCLASS()
class NEVERGONE_API UAISR_PreferCloserTargets : public UBattleAIScoringRule
{
	GENERATED_BODY()
	
public:
	virtual float ComputeRawScore(
		const FTeamCandidateAction& Candidate,
		const FAICandidateEvalContext& Context) const override;
	

};
