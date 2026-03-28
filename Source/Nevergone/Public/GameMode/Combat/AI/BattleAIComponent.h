#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/AIBattleTypes.h"
#include "BattleAIComponent.generated.h"

class UActionResolver;
class UBattleAIQueryService;
class UBattleAICandidateScorer;
class UBattleAIBehaviorProfile;

struct FActionContext;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class NEVERGONE_API UBattleAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBattleAIComponent();

	void Initialize(UBattleAIQueryService* InQueryService);
	void GatherCandidateActions(const FTeamTurnContext& TurnContext, TArray<FTeamCandidateAction>& OutCandidates) const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void GatherAbilityCandidates(const FTeamTurnContext& TurnContext, TArray<FTeamCandidateAction>& OutCandidates) const;
	void GatherMoveCandidates(const FTeamTurnContext& TurnContext, TArray<FTeamCandidateAction>& OutCandidates) const;
	void GatherEndTurnCandidate(const FTeamTurnContext& TurnContext, TArray<FTeamCandidateAction>& OutCandidates) const;

	bool BuildBaseContext(FActionContext& OutContext) const;
	bool BuildEvalContext(const FTeamTurnContext& TurnContext, FAICandidateEvalContext& OutContext) const;

	void AddCandidateIfValid(const FActionContext& Context, const FTeamTurnContext& TurnContext, TArray<FTeamCandidateAction>& OutCandidates) const;
	void RefreshScoringRules();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
	TObjectPtr<UBattleAIBehaviorProfile> BehaviorProfile;

private:
	UPROPERTY()
	TObjectPtr<UActionResolver> ActionResolver;

	UPROPERTY()
	TObjectPtr<UBattleAIQueryService> QueryService;

	UPROPERTY()
	TObjectPtr<UBattleAICandidateScorer> CandidateScorer;


};