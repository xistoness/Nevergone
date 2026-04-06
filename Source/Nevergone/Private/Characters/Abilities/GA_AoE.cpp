// Copyright Xyzto Works

#include "Characters/Abilities/GA_AoE.h"

#include "ActorComponents/BattleModeComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/TargetingRules/RangeRule.h"
#include "Characters/Abilities/AbilityPreview/Preview_AOE.h"
#include "Data/AbilityDefinition.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/BattleUnitState.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "GameMode/Combat/CombatFormulas.h"
#include "Level/GridManager.h"
#include "Nevergone.h"
#include "Types/BattleTypes.h"

UGA_AoE::UGA_AoE()
{
    PreviewRendererClass = UPreview_AOE::StaticClass();
}

FActionResult UGA_AoE::BuildPreview(const FActionContext& Context) const
{
    FActionResult Result;
    BuildAoEResult(Context, Result);
    return Result;
}

FActionResult UGA_AoE::BuildExecution(const FActionContext& Context) const
{
    FActionResult Result;
    BuildAoEResult(Context, Result);
    return Result;
}

bool UGA_AoE::BuildAoEResult(const FActionContext& Context, FActionResult& OutResult) const
{
    OutResult = FActionResult();
    
    if (!HasSufficientAP(Context))
    {
        return false;
    }

    ACharacterBase* SourceCharacter = Cast<ACharacterBase>(Context.SourceActor);
    if (!SourceCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AoE] BuildAoEResult: no source character"));
        return false;
    }

    if (!AbilityDefinition)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AoE] BuildAoEResult: AbilityDefinition is null on %s"),
            *GetNameSafe(SourceCharacter));
        return false;
    }

    // Range check: can the caster reach the targeted tile center?
    CompositeTargetingPolicy RangePolicy;
    RangePolicy.AddRule(MakeUnique<RangeRule>(GetMaxRange()));

    if (!RangePolicy.IsValid(Context))
    {
        UE_LOG(LogTemp, Log, TEXT("[GA_AoE] BuildAoEResult: target out of range for %s"),
            *GetNameSafe(SourceCharacter));
        return false;
    }

    UBattleModeComponent* BattleMode     = SourceCharacter->GetBattleModeComponent();
    UBattleState*         BattleStateRef = BattleMode ? BattleMode->GetBattleState() : nullptr;

    if (!BattleStateRef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AoE] BuildAoEResult: BattleState not available for %s"),
            *GetNameSafe(SourceCharacter));
        return false;
    }

    const FBattleUnitState* SourceState = BattleStateRef->FindUnitState(SourceCharacter);
    if (!SourceState)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_AoE] BuildAoEResult: SourceState not found for %s"),
            *GetNameSafe(SourceCharacter));
        return false;
    }

    TArray<ACharacterBase*> AffectedActors;
    if (!GatherAffectedActors(SourceCharacter, BattleStateRef, Context.HoveredGridCoord, AffectedActors)
        || AffectedActors.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("[GA_AoE] BuildAoEResult: no valid targets in AoE for %s"),
            *GetNameSafe(SourceCharacter));
        return false;
    }

    OutResult.bIsValid          = true;
    OutResult.bRequiresMovement = false;
    OutResult.ActionPointsCost  = GetActionPointsCost();
    OutResult.HitChance         = 1.0f;

    // Populate ExpectedDamage for UI preview and AI scoring
    if (AbilityDefinition->EffectType == EAbilityEffectType::Damage)
    {
        // Use first target as representative for the damage preview estimate
        const FBattleUnitState* FirstTargetState = BattleStateRef->FindUnitState(AffectedActors[0]);
        if (FirstTargetState)
        {
            OutResult.ExpectedDamage = FCombatFormulas::EstimateDamage(
                *SourceState, *FirstTargetState,
                AbilityDefinition->DamageSource,
                AbilityDefinition->DamageMultiplier,
                AbilityDefinition->BaseHitChance,
                AbilityDefinition->bCanMiss
            );
        }
    }

    for (ACharacterBase* Actor : AffectedActors)
    {
        OutResult.AffectedActors.Add(Actor);
    }

    return true;
}

bool UGA_AoE::GatherAffectedActors(
    ACharacterBase* SourceCharacter,
    UBattleState* BattleStateRef,
    const FIntPoint& CenterCoord,
    TArray<ACharacterBase*>& OutActors
) const
{
    if (!SourceCharacter || !BattleStateRef || !AbilityDefinition) { return false; }

    UWorld* World = SourceCharacter->GetWorld();
    if (!World) { return false; }

    UGridManager* Grid = World->GetSubsystem<UGridManager>();
    if (!Grid) { return false; }

    const FBattleUnitState* SourceState = BattleStateRef->FindUnitState(SourceCharacter);
    if (!SourceState) { return false; }

    const int32 Radius                 = AbilityDefinition->AoERadius;
    const EAbilityTargetFaction Faction = GetTargetFaction();

    for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
    {
        for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
        {
            const FIntPoint CandidateCoord(CenterCoord.X + OffsetX, CenterCoord.Y + OffsetY);
            ACharacterBase* Candidate = Cast<ACharacterBase>(Grid->GetActorAt(CandidateCoord));
            if (!Candidate) { continue; }

            // Alive check via BattleUnitState — not UnitStatsComponent
            const FBattleUnitState* CandidateState = BattleStateRef->FindUnitState(Candidate);
            if (!CandidateState || !CandidateState->IsAlive()) { continue; }

            // Team identity: BattleUnitState::Team is set at battle start from
            // CombatManager and is the correct source for faction checks during combat
            bool bFactionMatch = false;
            switch (Faction)
            {
            case EAbilityTargetFaction::Ally:
                bFactionMatch = CandidateState->Team == SourceState->Team;
                break;
            case EAbilityTargetFaction::Enemy:
                bFactionMatch = CandidateState->Team != SourceState->Team;
                break;
            case EAbilityTargetFaction::Any:
                bFactionMatch = true;
                break;
            }

            if (!bFactionMatch) { continue; }

            OutActors.Add(Candidate);
        }
    }

    return true;
}

void UGA_AoE::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData
)
{
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ACharacterBase* SourceCharacter = Cast<ACharacterBase>(ActorInfo->AvatarActor.Get());
    if (!SourceCharacter)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UBattleModeComponent* BattleMode = SourceCharacter->GetBattleModeComponent();
    if (!BattleMode)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const FActionContext& Context       = BattleMode->GetCurrentContext();
    const FActionResult ExecutionResult = BuildExecution(Context);

    if (!ExecutionResult.bIsValid)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    CachedCharacter        = SourceCharacter;
    CachedActionPointsCost = ExecutionResult.ActionPointsCost;
    CachedCenterCoord      = Context.HoveredGridCoord;
    CachedAffectedActors.Reset();

    for (AActor* Actor : ExecutionResult.AffectedActors)
    {
        if (ACharacterBase* Character = Cast<ACharacterBase>(Actor))
        {
            CachedAffectedActors.Add(Character);
        }
    }

    if (CachedAffectedActors.IsEmpty())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ApplyResolvedEffect();
    FinalizeAction();
}

void UGA_AoE::ApplyResolvedEffect()
{
    if (!CachedCharacter || !AbilityDefinition) { return; }

    UBattleModeComponent* BattleMode     = CachedCharacter->GetBattleModeComponent();
    UCombatEventBus*      Bus            = BattleMode ? BattleMode->GetCombatEventBus() : nullptr;
    UBattleState*         BattleStateRef = BattleMode ? BattleMode->GetBattleState()    : nullptr;

    if (!Bus || !BattleStateRef)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[GA_AoE] ApplyResolvedEffect: CombatEventBus or BattleState not found for %s"),
            *GetNameSafe(CachedCharacter));
        return;
    }

    const FBattleUnitState* SourceState = BattleStateRef->FindUnitState(CachedCharacter);
    if (!SourceState)
    {
        UE_LOG(LogTemp, Error, TEXT("[GA_AoE] ApplyResolvedEffect: source BattleUnitState not found"));
        return;
    }

    // Play the cast SFX once — not per target — to avoid audio spam on AoE hits
    PlayCastSFX(CachedCharacter);

    for (ACharacterBase* Target : CachedAffectedActors)
    {
        if (!IsValid(Target)) { continue; }

        // Re-check alive state each iteration: a previous NotifyDamageApplied broadcast
        // may have killed this target (OnUnitDied fires inside the broadcast chain),
        // invalidating the unit before we reach it in the loop.
        const FBattleUnitState* TargetState = BattleStateRef->FindUnitState(Target);
        if (!TargetState || !TargetState->IsAlive()) { continue; }

        switch (AbilityDefinition->EffectType)
        {
        case EAbilityEffectType::Damage:
        {
            const float FinalDamage = ResolveAttackDamage(*SourceState, *TargetState);

            Bus->NotifyDamageApplied(CachedCharacter, Target, FinalDamage);
            UE_LOG(LogTemp, Log, TEXT("[GA_AoE] Damage %.1f applied to %s"),
                FinalDamage, *GetNameSafe(Target));

            break;
        }
        case EAbilityEffectType::Heal:
        {
            const float FinalHeal = ResolveHeal(*SourceState);

            Bus->NotifyHealApplied(CachedCharacter, Target, FinalHeal);
            UE_LOG(LogTemp, Log, TEXT("[GA_AoE] Heal %.1f applied to %s"),
                FinalHeal, *GetNameSafe(Target));
            break;
        }
        case EAbilityEffectType::None:
            // No direct effect — status effects are handled below via ApplyOnHitStatusEffects
            break;
        }
    }
}

void UGA_AoE::FinalizeAction()
{
    FinalizeAbilityExecution();
}

void UGA_AoE::ApplyAbilityCompletionEffects()
{
    Super::ApplyAbilityCompletionEffects();

    if (!CachedCharacter) { return; }

    UBattleModeComponent* BattleMode = CachedCharacter->GetBattleModeComponent();
    if (!BattleMode) { return; }

    if (UBattleState* BattleStateRef = BattleMode->GetBattleState())
    {
        BattleStateRef->ConsumeActionPoints(CachedCharacter, CachedActionPointsCost);
    }

    // Collect raw pointers for the base class status effect helpers
    TArray<ACharacterBase*> HitActors;
    HitActors.Reserve(CachedAffectedActors.Num());
    for (const TObjectPtr<ACharacterBase>& Actor : CachedAffectedActors)
    {
        if (Actor) { HitActors.Add(Actor.Get()); }
    }

    ApplyOnHitStatusEffects(CachedCharacter, HitActors);
    ApplySelfStatusEffects(CachedCharacter);
    TryStartCooldown(CachedCharacter);
}

void UGA_AoE::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled
)
{
    CachedCharacter = nullptr;
    CachedAffectedActors.Reset();
    CachedActionPointsCost = 0;
    CachedCenterCoord      = FIntPoint::ZeroValue;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}