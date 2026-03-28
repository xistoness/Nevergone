// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameMode/Combat/AI/BattleAIScoringRule.h"
#include "AISR_PreferAbilityOverMove.generated.h"

/**
 * 
 */
UCLASS()
class NEVERGONE_API UAISR_PreferAbilityOverMove : public UBattleAIScoringRule
{
	GENERATED_BODY()

public:
	UAISR_PreferAbilityOverMove();

protected:
	UPROPERTY(EditAnywhere, Category="AI")
	float AbilityBonus = 1.f;

	UPROPERTY(EditAnywhere, Category="AI")
	float MovePenaltyWhenAttackExists = -0.5f;

	UPROPERTY(EditAnywhere, Category="AI")
	float MinimumUsefulDamage = 1.f;

public:
	virtual float ComputeRawScore(
		const FTeamCandidateAction& Candidate,
		const FAICandidateEvalContext& Context) const override;
};
