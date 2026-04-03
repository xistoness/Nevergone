// Copyright Xyzto Works


#include "GameMode/Combat/AI/ScoringRules/AISR_KillConfirm.h"

#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/AI/BattleAIQueryService.h"
#include "Types/AIBattleTypes.h"

UAISR_KillConfirm::UAISR_KillConfirm()
{
	Weight = 25.f;
	KillBonus = 1.f;
	AlreadyCoveredPenalty = -0.25f;
}

float UAISR_KillConfirm::ComputeRawScore(
	const FTeamCandidateAction& Candidate,
	const FAICandidateEvalContext& Context) const
{
	if (Candidate.ActionType != ETeamAIActionType::UseAbility) { return 0.f; }

	const ACharacterBase* Target = Candidate.Targeting.TargetActor;
	if (!Target || !Context.QueryService) { return 0.f; }

	UBattleState* BattleStateRef = Context.QueryService->GetBattleState();
	if (!BattleStateRef) { return 0.f; }

	const FBattleUnitState* TargetState = BattleStateRef->FindUnitState(const_cast<ACharacterBase*>(Target));
	if (!TargetState || !TargetState->IsAlive()) { return 0.f; }

	const FTeamTurnContext* TurnContext = Context.TurnContext;
	if (!TurnContext) { return 0.f; }

	const float ReservedDamage           = Context.QueryService->GetReservedDamageForTarget(Target, *TurnContext);
	const float RemainingHPAfterReserved = TargetState->CurrentHP - ReservedDamage;

	if (RemainingHPAfterReserved <= 0.f) { return AlreadyCoveredPenalty; }
	if (Candidate.ExpectedDamage >= RemainingHPAfterReserved) { return KillBonus; }

	return 0.f;
}
