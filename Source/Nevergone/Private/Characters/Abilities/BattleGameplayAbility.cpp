// Copyright Xyzto Works

#include "Characters/Abilities/BattleGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "ActorComponents/BattleModeComponent.h"
#include "Audio/AudioSubsystem.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/AbilityPreview/AbilityPreviewRenderer.h"
#include "Data/AbilityDefinition.h"
#include "Data/StatusEffectDefinition.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "GameMode/Combat/StatusEffectManager.h"
#include "GameMode/TurnManager.h"
#include "Nevergone.h"
#include "ActorComponents/MyAbilitySystemComponent.h"
#include "GameMode/Combat/CombatFormulas.h"
#include "Types/BattleTypes.h"

UBattleGameplayAbility::UBattleGameplayAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bActivateAbilityOnGranted = false;

    // Block activation while stunned.
    // Cooldown is enforced per-instance via IsOnCooldown() in HasSufficientAP(),
    // not via a shared ASC tag, so one ability's cooldown never blocks others.
    ActivationBlockedTags.AddTag(TAG_State_Stunned);
}

// Resolves the owning character from ActorInfo — works on both CDO and live instances.
static ACharacterBase* GetAvatarCharacter(const UGameplayAbility* Ability)
{
    if (!Ability) { return nullptr; }
    const FGameplayAbilityActorInfo* Info = Ability->GetCurrentActorInfo();
    if (!Info) { return nullptr; }
    return Cast<ACharacterBase>(Info->AvatarActor.Get());
}

bool UBattleGameplayAbility::IsOnCooldown() const
{
    if (!AbilityDefinition) { return false; }
    const ACharacterBase* Char = GetAvatarCharacter(this);
    if (!Char) { return false; }
    const UBattleModeComponent* BM = Char->GetBattleModeComponent();
    return BM ? BM->IsDefinitionOnCooldown(AbilityDefinition) : false;
}

int32 UBattleGameplayAbility::GetRemainingCooldownTurns() const
{
    if (!AbilityDefinition) { return 0; }
    const ACharacterBase* Char = GetAvatarCharacter(this);
    if (!Char) { return 0; }
    const UBattleModeComponent* BM = Char->GetBattleModeComponent();
    return BM ? BM->GetDefinitionCooldownTurns(AbilityDefinition) : 0;
}

FActionResult UBattleGameplayAbility::BuildPreview(const FActionContext& Context) const
{
    FActionResult Result;
    Result.bIsValid = false;
    return Result;
}

FActionResult UBattleGameplayAbility::BuildExecution(const FActionContext& Context) const
{
    return BuildPreview(Context);
}

EBattleAbilitySelectionMode UBattleGameplayAbility::GetSelectionMode() const
{
    return EBattleAbilitySelectionMode::SingleConfirm;
}

void UBattleGameplayAbility::FinalizeAbilityExecution()
{
    ACharacterBase* Character = Cast<ACharacterBase>(GetAvatarActorFromActorInfo());
    UBattleModeComponent* BattleMode = Character ? Character->GetBattleModeComponent() : nullptr;

    ApplyAbilityCompletionEffects();

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

    if (BattleMode)
    {
        BattleMode->HandleActionFinished();
    }
}

void UBattleGameplayAbility::ApplyAbilityCompletionEffects()
{
    // Default: do nothing
}

void UBattleGameplayAbility::CleanupAbilityDelegates()
{
}

TSubclassOf<UAbilityPreviewRenderer> UBattleGameplayAbility::GetPreviewClass() const
{
    return PreviewRendererClass;
}

// ---------------------------------------------------------------------------
// Definition helpers
// ---------------------------------------------------------------------------

int32 UBattleGameplayAbility::GetActionPointsCost() const
{
    if (!AbilityDefinition)
    {
        UE_LOG(LogNevergone, Warning, TEXT("[BattleGameplayAbility] GetActionPointsCost: AbilityDefinition null on %s"), *GetName());
        return 1;
    }
    return AbilityDefinition->ActionPointsCost;
}

int32 UBattleGameplayAbility::GetMaxRange() const
{
    if (!AbilityDefinition)
    {
        UE_LOG(LogNevergone, Warning, TEXT("[BattleGameplayAbility] GetMaxRange: AbilityDefinition null on %s"), *GetName());
        return 1;
    }
    return AbilityDefinition->MaxRange;
}

EAbilityTargetFaction UBattleGameplayAbility::GetTargetFaction() const
{
    if (!AbilityDefinition) { return EAbilityTargetFaction::Enemy; }
    return AbilityDefinition->TargetFaction;
}

float UBattleGameplayAbility::ResolveAttackDamage(
    const FBattleUnitState& SourceState,
    const FBattleUnitState& TargetState
) const
{
    if (!AbilityDefinition) { return 0.f; }

    const float HitChance = FCombatFormulas::ComputeHitChance(
        SourceState, TargetState,
        AbilityDefinition->BaseHitChance,
        AbilityDefinition->bCanMiss
    );

    if (!FCombatFormulas::RollHit(HitChance))
    {
        UE_LOG(LogNevergone, Log, TEXT("[BattleGameplayAbility] %s: attack missed"), *GetName());
        return 0.f;
    }

    float Damage = FCombatFormulas::ComputeDamage(
        SourceState, TargetState,
        AbilityDefinition->DamageSource,
        AbilityDefinition->DamageMultiplier
    );

    if (FCombatFormulas::RollCrit(SourceState))
    {
        Damage = FCombatFormulas::ApplyCritMultiplier(Damage);
        UE_LOG(LogNevergone, Log, TEXT("[BattleGameplayAbility] %s: CRITICAL HIT — damage %.1f"), *GetName(), Damage);
    }

    return Damage;
}

float UBattleGameplayAbility::ResolveHeal(const FBattleUnitState& SourceState) const
{
    if (!AbilityDefinition) { return 0.f; }
    return FCombatFormulas::ComputeHeal(SourceState, AbilityDefinition->HealMultiplier);
}

// ---------------------------------------------------------------------------
// Status Effects
// ---------------------------------------------------------------------------

void UBattleGameplayAbility::ApplyOnHitStatusEffects(
    ACharacterBase* SourceCharacter,
    const TArray<ACharacterBase*>& HitActors
)
{
    if (!AbilityDefinition || AbilityDefinition->OnHitStatusEffects.IsEmpty())
    {
        return;
    }

    UBattleModeComponent* BattleMode = SourceCharacter
        ? SourceCharacter->GetBattleModeComponent()
        : nullptr;
    UStatusEffectManager* StatusManager = BattleMode
        ? BattleMode->GetStatusEffectManager()
        : nullptr;

    if (!StatusManager)
    {
        UE_LOG(LogNevergone, Warning,
            TEXT("[BattleGameplayAbility] ApplyOnHitStatusEffects: StatusEffectManager not found on %s — skipped"),
            *GetNameSafe(SourceCharacter));
        return;
    }

    // Resolve the caster stat that will be snapshotted for PercentOfCasterStat
    // and Shield formulas. The stat mirrors this ability's own DamageSource so
    // Poison from a Magical ability scales off MagicalPower, Physical off PhysicalAttack, etc.
    const int32 CasterStatSnapshot = ResolveCasterStatSnapshot(SourceCharacter);

    for (ACharacterBase* Target : HitActors)
    {
        if (!Target) { continue; }

        for (UStatusEffectDefinition* Definition : AbilityDefinition->OnHitStatusEffects)
        {
            if (!Definition) { continue; }
            StatusManager->ApplyStatusEffect(SourceCharacter, Target, Definition, CasterStatSnapshot);
        }
    }
}

void UBattleGameplayAbility::ApplySelfStatusEffects(ACharacterBase* SourceCharacter)
{
    if (!AbilityDefinition || AbilityDefinition->SelfStatusEffects.IsEmpty())
    {
        return;
    }

    UBattleModeComponent* BattleMode = SourceCharacter
        ? SourceCharacter->GetBattleModeComponent()
        : nullptr;
    UStatusEffectManager* StatusManager = BattleMode
        ? BattleMode->GetStatusEffectManager()
        : nullptr;

    if (!StatusManager)
    {
        UE_LOG(LogNevergone, Warning,
            TEXT("[BattleGameplayAbility] ApplySelfStatusEffects: StatusEffectManager not found on %s — skipped"),
            *GetNameSafe(SourceCharacter));
        return;
    }

    const int32 CasterStatSnapshot = ResolveCasterStatSnapshot(SourceCharacter);

    for (UStatusEffectDefinition* Definition : AbilityDefinition->SelfStatusEffects)
    {
        if (!Definition) { continue; }
        StatusManager->ApplyStatusEffect(SourceCharacter, SourceCharacter, Definition, CasterStatSnapshot);
    }
}

int32 UBattleGameplayAbility::ResolveCasterStatSnapshot(ACharacterBase* SourceCharacter) const
{
    if (!AbilityDefinition || !SourceCharacter) { return 0; }

    UBattleModeComponent* BattleMode = SourceCharacter->GetBattleModeComponent();
    const UBattleState* BattleStateRef = BattleMode ? BattleMode->GetBattleState() : nullptr;
    if (!BattleStateRef) { return 0; }

    const FBattleUnitState* SourceState = BattleStateRef->FindUnitState(SourceCharacter);
    if (!SourceState) { return 0; }

    // Mirror the ability's own DamageSource so that status DoTs and Shields scale
    // off the same stat the ability itself uses for direct damage.
    switch (AbilityDefinition->DamageSource)
    {
        case EAbilityDamageSource::Physical: return SourceState->PhysicalAttack;
        case EAbilityDamageSource::Ranged:   return SourceState->RangedAttack;
        case EAbilityDamageSource::Magical:  return SourceState->MagicalPower;
    }

    return 0;
}

bool UBattleGameplayAbility::HasSufficientAP(const FActionContext& Context) const
{
    // Cooldown check — each ability instance tracks its own cooldown independently.
    // This prevents one ability's cooldown from blocking all others on the unit.
    if (IsOnCooldown())
    {
        return false;
    }

    const ACharacterBase* Source = Cast<ACharacterBase>(Context.SourceActor);
    if (!Source) { return false; }
 
    const UBattleModeComponent* BattleMode = Source->GetBattleModeComponent();
    const UBattleState* BattleStateRef = BattleMode ? BattleMode->GetBattleState() : nullptr;
    if (!BattleStateRef) { return false; }
 
    const FBattleUnitState* UnitState = BattleStateRef->FindUnitState(
        const_cast<ACharacterBase*>(Source)
    );
    if (!UnitState) { return false; }
 
    const int32 Cost = GetActionPointsCost();
    const bool bSufficient = UnitState->CurrentActionPoints >= Cost;
 
    if (!bSufficient)
    {
        UE_LOG(LogNevergone, Log,
            TEXT("[BattleGameplayAbility] HasSufficientAP: %s has %d AP, needs %d -- blocked"),
            *GetNameSafe(Source), UnitState->CurrentActionPoints, Cost);
    }
 
    return bSufficient;
}

// ---------------------------------------------------------------------------
// Cooldown
// ---------------------------------------------------------------------------

void UBattleGameplayAbility::TryStartCooldown(ACharacterBase* SourceCharacter)
{
    if (!AbilityDefinition || AbilityDefinition->CooldownTurns <= 0)
    {
        return;
    }

    if (!SourceCharacter)
    {
        UE_LOG(LogNevergone, Warning,
            TEXT("[BattleGameplayAbility] TryStartCooldown: SourceCharacter is null on %s — skipping"),
            *GetName());
        return;
    }

    UBattleModeComponent* BattleMode = SourceCharacter->GetBattleModeComponent();
    UTurnManager* TurnManager = BattleMode ? BattleMode->GetTurnManager() : nullptr;

    if (!TurnManager)
    {
        UE_LOG(LogNevergone, Warning,
            TEXT("[BattleGameplayAbility] TryStartCooldown: TurnManager not available for %s — skipping"),
            *GetNameSafe(SourceCharacter));
        return;
    }

    UAbilitySystemComponent* ASC = SourceCharacter->GetAbilitySystemComponent();
    if (!ASC)
    {
        UE_LOG(LogNevergone, Warning,
            TEXT("[BattleGameplayAbility] TryStartCooldown: ASC not found on %s — skipping"),
            *GetNameSafe(SourceCharacter));
        return;
    }

    CooldownOwner = SourceCharacter;
    
    if (!BattleMode) { return; }
    // Store CooldownTurns + 1 so the decrement that fires at the start of the
    // NEXT player turn brings it to CooldownTurns, not CooldownTurns - 1.
    // Model A: cooldown 2 used on turn 1 -> stored as 3, decrements to 2 on
    // turn 2 start (still blocked), to 1 on turn 3 start (blocked), to 0 on
    // turn 4 start -> expires -> available turn 4. 2 full turns skipped.
    BattleMode->StartDefinitionCooldown(AbilityDefinition, AbilityDefinition->CooldownTurns + 1);

    // Subscribe to turn ticks to call TickDefinitionCooldowns each player turn.
    CooldownTurnHandle = TurnManager->OnTurnStateChanged.AddUObject(
        this, &UBattleGameplayAbility::OnTurnStateChanged
    );

    UE_LOG(LogNevergone, Log,
        TEXT("[BattleGameplayAbility] %s cooldown started for %s — %d turns remaining"),
        *GetName(), *GetNameSafe(SourceCharacter), AbilityDefinition->CooldownTurns);
}

void UBattleGameplayAbility::OnTurnStateChanged(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase)
{
    // Decrement once per player turn start — one full cycle = Player + Enemy
    if (NewOwner != EBattleTurnOwner::Player || NewPhase != EBattleTurnPhase::AwaitingOrders)
    {
        return;
    }

    // Tick the definition cooldown map on the owning unit.
    // TickDefinitionCooldowns decrements all entries; we unsubscribe once ours expires.
    if (CooldownOwner)
    {
        if (UBattleModeComponent* BM = CooldownOwner->GetBattleModeComponent())
        {
            BM->TickDefinitionCooldowns();

            // Unsubscribe when our definition is no longer on cooldown
            if (!BM->IsDefinitionOnCooldown(AbilityDefinition))
            {
                ClearCooldown();
            }
        }
    }
}

void UBattleGameplayAbility::ClearCooldown()
{
    if (CooldownTurnHandle.IsValid())
    {
        if (CooldownOwner)
        {
            if (UBattleModeComponent* BattleMode = CooldownOwner->GetBattleModeComponent())
            {
                if (UTurnManager* TurnManager = BattleMode->GetTurnManager())
                {
                    TurnManager->OnTurnStateChanged.Remove(CooldownTurnHandle);
                }
            }
        }
        CooldownTurnHandle.Reset();
    }

    if (CooldownOwner)
    {
        UE_LOG(LogNevergone, Log,
            TEXT("[BattleGameplayAbility] %s cooldown expired for %s -- ability available again"),
            *GetName(), *GetNameSafe(CooldownOwner));
    }

    CooldownOwner = nullptr;
}

void UBattleGameplayAbility::PlayCastSFX(ACharacterBase* SourceCharacter) const
{
    if (!SourceCharacter || !AbilityDefinition || !AbilityDefinition->CastSound)
    {
        return;
    }
 
    UWorld* World = SourceCharacter->GetWorld();
    if (!World)
    {
        return;
    }
 
    UAudioSubsystem* Audio = World->GetGameInstance()->GetSubsystem<UAudioSubsystem>();
    if (!Audio)
    {
        return;
    }
 
    Audio->PlaySFXAtLocation(AbilityDefinition->CastSound, SourceCharacter->GetActorLocation());
 
    UE_LOG(LogNevergone, Verbose,
        TEXT("[%s] Cast SFX played at source '%s'"),
        *GetName(), *GetNameSafe(SourceCharacter));
}

void UBattleGameplayAbility::PlayImpactSFX(ACharacterBase* TargetCharacter) const
{
    if (!TargetCharacter)
    {
        return;
    }
 
    UWorld* World = TargetCharacter->GetWorld();
    if (!World)
    {
        return;
    }
 
    UAudioSubsystem* Audio = World->GetGameInstance()->GetSubsystem<UAudioSubsystem>();
    if (!Audio)
    {
        return;
    }
 
    // Use the ability-specific ImpactSound if assigned;
    // otherwise the AudioSubsystem's DefaultHitSFX acts as the fallback
    USoundBase* Sound = (AbilityDefinition && AbilityDefinition->ImpactSound)
        ? AbilityDefinition->ImpactSound.Get()
        : nullptr;
 
    if (Sound)
    {
        Audio->PlaySFXAtLocation(Sound, TargetCharacter->GetActorLocation());
 
        UE_LOG(LogNevergone, Verbose,
            TEXT("[%s] Impact SFX played at target '%s'"),
            *GetName(), *GetNameSafe(TargetCharacter));
    }
    // If Sound is null, AudioSubsystem's HandleDamageApplied (via EventBus) will
    // fire DefaultHitSFX automatically — no double-play, no action needed here.
}