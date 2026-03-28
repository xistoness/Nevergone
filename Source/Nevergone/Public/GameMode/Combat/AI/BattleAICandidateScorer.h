// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/AIBattleTypes.h"
#include "BattleAICandidateScorer.generated.h"

class UBattleAIBehaviorProfile;
class UBattleAIScoringRule;

UCLASS()
class NEVERGONE_API UBattleAICandidateScorer : public UObject
{
	GENERATED_BODY()
	
public:
	float ScoreCandidate(
		const FTeamCandidateAction& Candidate,
		const FAICandidateEvalContext& Context) const;

	void SetRules(const UBattleAIBehaviorProfile* Profile);

private:
	UPROPERTY()
	TArray<TObjectPtr<UBattleAIScoringRule>> Rules;
	
};
