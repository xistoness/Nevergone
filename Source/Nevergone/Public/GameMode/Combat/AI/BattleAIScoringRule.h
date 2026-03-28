// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/AIBattleTypes.h"
#include "BattleAIScoringRule.generated.h"


UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class NEVERGONE_API UBattleAIScoringRule : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
	float Weight = 1.f;

	virtual float ComputeRawScore(const FTeamCandidateAction& Candidate, const FAICandidateEvalContext& Context) const PURE_VIRTUAL(UBattleAIScoringRule::ComputeRawScore, return 0.f;);

	float Evaluate(const FTeamCandidateAction& Candidate, const FAICandidateEvalContext& Context) const;

};
