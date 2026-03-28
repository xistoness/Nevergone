// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameMode/Combat/AI/BattleAIScoringRule.h"
#include "AISR_ExpectedDamage.generated.h"

/**
 * 
 */
UCLASS()
class NEVERGONE_API UAISR_ExpectedDamage : public UBattleAIScoringRule
{
	GENERATED_BODY()
	
public:
	UAISR_ExpectedDamage();

protected:
	UPROPERTY(EditAnywhere, Category="AI")
	float MinDamageForScore = 0.f;

	UPROPERTY(EditAnywhere, Category="AI")
	float MaxDamageForFullScore = 25.f;

	UPROPERTY(EditAnywhere, Category="AI")
	float MaxRawScore = 1.f;

public:
	virtual float ComputeRawScore(
		const FTeamCandidateAction& Candidate,
		const FAICandidateEvalContext& Context) const override;
	
};
