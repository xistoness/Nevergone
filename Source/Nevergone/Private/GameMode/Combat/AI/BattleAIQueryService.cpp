// Copyright Xyzto Works

#include "GameMode/Combat/AI/BattleAIQueryService.h"

#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "Data/AbilityDefinition.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/BattleUnitState.h"
#include "GameMode/Combat/CombatFormulas.h"
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
    if (!IsUnitAlive(Unit)) { return false; }

    if (!BattleState)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleAIQueryService] BattleState not set — CanUnitStillAct returning false"));
        return false;
    }

    const FBattleUnitState* State = BattleState->FindUnitState(const_cast<ACharacterBase*>(Unit));
    return State && State->CurrentActionPoints > 0;
}

bool UBattleAIQueryService::IsUnitAlive(const ACharacterBase* Unit) const
{
    if (!IsValid(Unit) || !BattleState) { return false; }

    const FBattleUnitState* State = BattleState->FindUnitState(const_cast<ACharacterBase*>(Unit));
    return State && State->IsAlive();
}

bool UBattleAIQueryService::AreUnitsAllies(const ACharacterBase* UnitA, const ACharacterBase* UnitB) const
{
    if (!UnitA || !UnitB || !BattleState) { return false; }

    // Team identity is stored in BattleUnitState — use it directly
    const FBattleUnitState* StateA = BattleState->FindUnitState(const_cast<ACharacterBase*>(UnitA));
    const FBattleUnitState* StateB = BattleState->FindUnitState(const_cast<ACharacterBase*>(UnitB));

    if (!StateA || !StateB) { return false; }

    return StateA->Team == StateB->Team;
}

TArray<FIntPoint> UBattleAIQueryService::GetReachableTiles(const ACharacterBase* Unit) const
{
    TArray<FIntPoint> ReachableTiles;

    if (!CanUnitStillAct(Unit) || !Unit->GetWorld() || !BattleState)
    {
        return ReachableTiles;
    }

    const FBattleUnitState* UnitState = BattleState->FindUnitState(const_cast<ACharacterBase*>(Unit));
    if (!UnitState) { return ReachableTiles; }

    UGridManager* Grid = Unit->GetWorld()->GetSubsystem<UGridManager>();
    if (!Grid) { return ReachableTiles; }

    const FIntPoint SourceCoord = Grid->WorldToGrid(Unit->GetActorLocation());
    const int32 MaxRange        = UnitState->MovementRange * UnitState->CurrentActionPoints;

    for (int32 X = SourceCoord.X - MaxRange; X <= SourceCoord.X + MaxRange; ++X)
    {
        for (int32 Y = SourceCoord.Y - MaxRange; Y <= SourceCoord.Y + MaxRange; ++Y)
        {
            const FIntPoint Candidate(X, Y);
            if (!Grid->IsValidCoord(Candidate)) { continue; }

            const FGridTile* Tile = Grid->GetTile(Candidate);
            if (!Tile || Tile->bBlocked) { continue; }

            AActor* Occupant = Grid->GetActorAt(Candidate);
            if (Occupant && Occupant != Unit) { continue; }

            const int32 PathCost = Grid->CalculatePathCost(SourceCoord, Candidate, UnitState->TraversalParams);
            if (PathCost == INDEX_NONE || PathCost > MaxRange) { continue; }

            ReachableTiles.Add(Candidate);
        }
    }

    return ReachableTiles;
}

TArray<ACharacterBase*> UBattleAIQueryService::GetValidTargetsForAbility(
    const ACharacterBase* Unit,
    TSubclassOf<UBattleGameplayAbility> AbilityClass,
    const UAbilityDefinition* Definition
) const
{
    TArray<ACharacterBase*> Result;

    if (!CanUnitStillAct(Unit) || !AbilityClass || !Definition || !Unit->GetWorld() || !BattleState)
    {
        return Result;
    }

    const FBattleUnitState* UnitState = BattleState->FindUnitState(const_cast<ACharacterBase*>(Unit));
    if (!UnitState) { return Result; }

    UGridManager* Grid = Unit->GetWorld()->GetSubsystem<UGridManager>();
    if (!Grid) { return Result; }

    // Read TargetFaction directly from the passed Definition — the CDO does not have it set
    // because the project uses a template-class pattern (one C++ class, many Definitions).
    TArray<ACharacterBase*> CandidateTargets;

    switch (Definition->TargetFaction)
    {
    case EAbilityTargetFaction::Enemy:
        CandidateTargets = GetEnemyUnitsForTeam(UnitState->Team);
        break;
    case EAbilityTargetFaction::Ally:
        CandidateTargets = GetAlliedUnitsForTeam(UnitState->Team);
        break;
    case EAbilityTargetFaction::Any:
        CandidateTargets = GetAlliedUnitsForTeam(UnitState->Team);
        CandidateTargets.Append(GetEnemyUnitsForTeam(UnitState->Team));
        break;
    }

    UActionResolver* Resolver = NewObject<UActionResolver>(const_cast<UBattleAIQueryService*>(this));
    if (!Resolver) { return Result; }

    for (ACharacterBase* Target : CandidateTargets)
    {
        if (!Target) { continue; }

        FActionContext Context;
        Context.SourceActor          = const_cast<ACharacterBase*>(Unit);
        Context.TargetActor          = Target;
        Context.SourceGridCoord      = Grid->WorldToGrid(Unit->GetActorLocation());
        Context.HoveredGridCoord     = Grid->WorldToGrid(Target->GetActorLocation());
        Context.SourceWorldPosition  = Unit->GetActorLocation();
        Context.HoveredWorldPosition = Target->GetActorLocation();
        Context.TraversalParams      = UnitState->TraversalParams;
        Context.SelectionPhase       = EBattleActionSelectionPhase::SelectingTarget;

        const FActionResult ResultPreview = Resolver->ResolvePreview(AbilityClass, Context);
        if (ResultPreview.bIsValid)
        {
            Result.Add(Target);
        }
    }

    return Result;
}

float UBattleAIQueryService::EstimateDamage(
    const ACharacterBase* SourceUnit,
    const ACharacterBase* TargetUnit,
    TSubclassOf<UBattleGameplayAbility> AbilityClass,
    const UAbilityDefinition* Definition
) const
{
    if (!SourceUnit || !TargetUnit || !AbilityClass || !Definition || !BattleState) { return 0.f; }

    const FBattleUnitState* SourceState = BattleState->FindUnitState(const_cast<ACharacterBase*>(SourceUnit));
    const FBattleUnitState* TargetState = BattleState->FindUnitState(const_cast<ACharacterBase*>(TargetUnit));

    if (!SourceState || !TargetState) { return 0.f; }

    // Heal abilities don't deal damage — return 0 so damage-based scoring rules ignore them
    if (Definition->EffectType == EAbilityEffectType::Heal)
    {
        return 0.f;
    }

    return FCombatFormulas::EstimateDamage(
        *SourceState, *TargetState,
        Definition->DamageSource,
        Definition->DamageMultiplier,
        Definition->BaseHitChance,
        Definition->bCanMiss
    );
}

float UBattleAIQueryService::GetDistanceScore(const ACharacterBase* SourceUnit, const ACharacterBase* TargetUnit) const
{
    if (!SourceUnit || !TargetUnit || !SourceUnit->GetWorld()) { return 0.f; }

    UGridManager* Grid = SourceUnit->GetWorld()->GetSubsystem<UGridManager>();
    if (!Grid) { return 0.f; }

    const FIntPoint SourceCoord = Grid->WorldToGrid(SourceUnit->GetActorLocation());
    const FIntPoint TargetCoord = Grid->WorldToGrid(TargetUnit->GetActorLocation());
    const int32 Distance = FMath::Max(
        FMath::Abs(SourceCoord.X - TargetCoord.X),
        FMath::Abs(SourceCoord.Y - TargetCoord.Y));

    return 1.f / FMath::Max(1, Distance);
}

bool UBattleAIQueryService::IsTileReserved(const FIntPoint& Tile, const FTeamTurnContext& TurnContext) const
{
    return TurnContext.ReservedTiles.Contains(Tile);
}

float UBattleAIQueryService::GetReservedDamageForTarget(
    const ACharacterBase* Target,
    const FTeamTurnContext& TurnContext) const
{
    if (const float* ReservedDamage = TurnContext.ReservedDamageByTarget.Find(const_cast<ACharacterBase*>(Target)))
    {
        return *ReservedDamage;
    }
    return 0.f;
}

void UBattleAIQueryService::SetBattleState(UBattleState* InBattleState)
{
    BattleState = InBattleState;
}

bool UBattleAIQueryService::DoesUnitBelongToTeam(const ACharacterBase* Unit, EBattleUnitTeam Team) const
{
    if (!Unit || !BattleState) { return false; }
    
    const FBattleUnitState* State = BattleState->FindUnitState(const_cast<ACharacterBase*>(Unit));
    return State && State->Team == Team;
}