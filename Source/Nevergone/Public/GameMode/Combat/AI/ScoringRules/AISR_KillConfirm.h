// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameMode/Combat/AI/BattleAIScoringRule.h"
#include "AISR_KillConfirm.generated.h"

/**
 * 
 */
UCLASS()
class NEVERGONE_API UAISR_KillConfirm : public UBattleAIScoringRule
{
	GENERATED_BODY()

public:
	UAISR_KillConfirm();

protected:
	UPROPERTY(EditAnywhere, Category="AI")
	float KillBonus = 1.f;

	UPROPERTY(EditAnywhere, Category="AI")
	float AlreadyCoveredPenalty = -0.75f;

public:
	virtual float ComputeRawScore(
		const FTeamCandidateAction& Candidate,
		const FAICandidateEvalContext& Context) const override;
};
