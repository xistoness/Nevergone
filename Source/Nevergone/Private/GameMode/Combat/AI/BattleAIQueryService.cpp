// Copyright Xyzto Works

#include "GameMode/Combat/AI/BattleAIQueryService.h"

#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "GameMode/Combat/Resolvers/ActionResolver.h"
#include "Level/GridManager.h"

void UBattleAIQueryService::Initialize(const TArray<AActor*>& InCombatants)
{
	Combatants = InCombatants;
}

TArray<ACharacterBase*> UBattleAIQueryService::GetAlliedUnitsForTeam(EBattleUnitTeam Team) const
{
	TArray<ACharacterBase*> Result;

	for (AActor* Actor : Combatants)
	{
		ACharacterBase* Unit = Cast<ACharacterBase>(Actor);
		if (!Unit || !IsUnitAlive(Unit) || !DoesUnitBelongToTeam(Unit, Team))
		{
			continue;
		}

		Result.Add(Unit);
	}

	return Result;
}

TArray<ACharacterBase*> UBattleAIQueryService::GetEnemyUnitsForTeam(EBattleUnitTeam Team) const
{
	const EBattleUnitTeam EnemyTeam = (Team == EBattleUnitTeam::Enemy)
		? EBattleUnitTeam::Ally
		: EBattleUnitTeam::Enemy;

	return GetAlliedUnitsForTeam(EnemyTeam);
}

bool UBattleAIQueryService::CanUnitStillAct(const ACharacterBase* Unit) const
{
	if (!IsUnitAlive(Unit))
	{
		return false;
	}

	const UUnitStatsComponent* Stats = Unit->GetUnitStats();
	return Stats && Stats->GetCurrentActionPoints() > 0;
}

bool UBattleAIQueryService::IsUnitAlive(const ACharacterBase* Unit) const
{
	if (!IsValid(Unit))
	{
		return false;
	}

	const UUnitStatsComponent* Stats = Unit->GetUnitStats();
	return Stats && Stats->IsAlive();
}

bool UBattleAIQueryService::AreUnitsAllies(const ACharacterBase* UnitA, const ACharacterBase* UnitB) const
{
	if (!UnitA || !UnitB)
	{
		return false;
	}

	const UUnitStatsComponent* StatsA = UnitA->GetUnitStats();
	const UUnitStatsComponent* StatsB = UnitB->GetUnitStats();
	if (!StatsA || !StatsB)
	{
		return false;
	}

	return StatsA->GetAllyTeam() == StatsB->GetAllyTeam();
}

TArray<FIntPoint> UBattleAIQueryService::GetReachableTiles(const ACharacterBase* Unit) const
{
	TArray<FIntPoint> ReachableTiles;

	if (!CanUnitStillAct(Unit) || !Unit->GetWorld())
	{
		return ReachableTiles;
	}

	const UUnitStatsComponent* Stats = Unit->GetUnitStats();
	UGridManager* Grid = Unit->GetWorld()->GetSubsystem<UGridManager>();
	if (!Stats || !Grid)
	{
		return ReachableTiles;
	}

	const FIntPoint SourceCoord = Grid->WorldToGrid(Unit->GetActorLocation());
	const int32 MaxRange = FMath::Max(0, Stats->GetSpeed() * Stats->GetCurrentActionPoints());
	const FGridTraversalParams TraversalParams = Stats->GetTraversalParams();

	for (int32 X = SourceCoord.X - MaxRange; X <= SourceCoord.X + MaxRange; ++X)
	{
		for (int32 Y = SourceCoord.Y - MaxRange; Y <= SourceCoord.Y + MaxRange; ++Y)
		{
			const FIntPoint Candidate(X, Y);
			if (!Grid->IsValidCoord(Candidate))
			{
				continue;
			}

			const FGridTile* Tile = Grid->GetTile(Candidate);
			if (!Tile || Tile->bBlocked)
			{
				continue;
			}

			AActor* Occupant = Grid->GetActorAt(Candidate);
			if (Occupant && Occupant != Unit)
			{
				continue;
			}

			const int32 PathCost = Grid->CalculatePathCost(SourceCoord, Candidate, TraversalParams);
			if (PathCost == INDEX_NONE || PathCost > MaxRange)
			{
				continue;
			}

			ReachableTiles.Add(Candidate);
		}
	}

	return ReachableTiles;
}

TArray<ACharacterBase*> UBattleAIQueryService::GetValidTargetsForAbility(
	const ACharacterBase* Unit,
	TSubclassOf<UBattleGameplayAbility> AbilityClass
) const
{
	TArray<ACharacterBase*> Result;

	if (!CanUnitStillAct(Unit) || !AbilityClass || !Unit->GetWorld())
	{
		return Result;
	}

	UGridManager* Grid = Unit->GetWorld()->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		return Result;
	}

	const UUnitStatsComponent* SourceStats = Unit->GetUnitStats();
	if (!SourceStats)
	{
		return Result;
	}

	const EBattleUnitTeam Team = SourceStats->GetAllyTeam() == 1 ? EBattleUnitTeam::Enemy : EBattleUnitTeam::Ally;
	const TArray<ACharacterBase*> Enemies = GetEnemyUnitsForTeam(Team);

	UActionResolver* Resolver = NewObject<UActionResolver>(const_cast<UBattleAIQueryService*>(this));
	if (!Resolver)
	{
		return Result;
	}

	for (ACharacterBase* Enemy : Enemies)
	{
		if (!Enemy)
		{
			continue;
		}

		FActionContext Context;
		Context.SourceActor = const_cast<ACharacterBase*>(Unit);
		Context.TargetActor = Enemy;
		Context.SourceGridCoord = Grid->WorldToGrid(Unit->GetActorLocation());
		Context.HoveredGridCoord = Grid->WorldToGrid(Enemy->GetActorLocation());
		Context.SourceWorldPosition = Unit->GetActorLocation();
		Context.HoveredWorldPosition = Enemy->GetActorLocation();
		Context.TraversalParams = SourceStats->GetTraversalParams();
		Context.SelectionPhase = EBattleActionSelectionPhase::SelectingTarget;

		const FActionResult ResultPreview = Resolver->ResolvePreview(AbilityClass, Context);
		if (ResultPreview.bIsValid)
		{
			Result.Add(Enemy);
		}
	}

	return Result;
}

float UBattleAIQueryService::EstimateDamage(
	const ACharacterBase* SourceUnit,
	const ACharacterBase* TargetUnit,
	TSubclassOf<UBattleGameplayAbility> AbilityClass
) const
{
	if (!SourceUnit || !TargetUnit || !AbilityClass)
	{
		return 0.f;
	}

	const UBattleGameplayAbility* AbilityCDO = AbilityClass->GetDefaultObject<UBattleGameplayAbility>();
	if (!AbilityCDO)
	{
		return 0.f;
	}

	const UUnitStatsComponent* SourceStats = SourceUnit->GetUnitStats();
	if (!SourceStats)
	{
		return 0.f;
	}

	const FString AbilityName = AbilityClass->GetName();
	if (AbilityName.Contains(TEXT("Attack_Melee")))
	{
		return SourceStats->GetMeleeAttack();
	}

	return 0.f;
}

float UBattleAIQueryService::GetDistanceScore(const ACharacterBase* SourceUnit, const ACharacterBase* TargetUnit) const
{
	if (!SourceUnit || !TargetUnit || !SourceUnit->GetWorld())
	{
		return 0.f;
	}

	UGridManager* Grid = SourceUnit->GetWorld()->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		return 0.f;
	}

	const FIntPoint SourceCoord = Grid->WorldToGrid(SourceUnit->GetActorLocation());
	const FIntPoint TargetCoord = Grid->WorldToGrid(TargetUnit->GetActorLocation());
	const int32 Distance = FMath::Max(FMath::Abs(SourceCoord.X - TargetCoord.X), FMath::Abs(SourceCoord.Y - TargetCoord.Y));

	return 1.f / FMath::Max(1, Distance);
}

bool UBattleAIQueryService::IsTileReserved(const FIntPoint& Tile, const FTeamTurnContext& TurnContext) const
{
	return TurnContext.ReservedTiles.Contains(Tile);
}

float UBattleAIQueryService::GetReservedDamageForTarget(const ACharacterBase* Target, const FTeamTurnContext& TurnContext) const
{
	if (const float* ReservedDamage = TurnContext.ReservedDamageByTarget.Find(const_cast<ACharacterBase*>(Target)))
	{
		return *ReservedDamage;
	}

	return 0.f;
}

bool UBattleAIQueryService::DoesUnitBelongToTeam(const ACharacterBase* Unit, EBattleUnitTeam Team) const
{
	if (!Unit)
	{
		return false;
	}

	const UUnitStatsComponent* Stats = Unit->GetUnitStats();
	if (!Stats)
	{
		return false;
	}

	const int32 ExpectedAllyTeamIndex = Team == EBattleUnitTeam::Enemy ? 1 : 0;
	return Stats->GetAllyTeam() == ExpectedAllyTeamIndex;
}
