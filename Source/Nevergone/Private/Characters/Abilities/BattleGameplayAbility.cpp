// Copyright Xyzto Works

#include "Characters/Abilities/BattleGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "ActorComponents/BattleModeComponent.h"
#include "Audio/AudioSubsystem.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/AbilityPreview/AbilityPreviewRenderer.h"
#include "Data/AbilityDefinition.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "GameMode/TurnManager.h"
#include "Nevergone.h"
#include "ActorComponents/MyAbilitySystemComponent.h"
#include "GameMode/Combat/CombatFormulas.h"
#include "Types/BattleTypes.h"

UBattleGameplayAbility::UBattleGameplayAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bActivateAbilityOnGranted = false;

    // All abilities are blocked while stunned or while this instance is on cooldown
    ActivationBlockedTags.AddTag(TAG_State_Stunned);
    ActivationBlockedTags.AddTag(TAG_State_AbilityOnCooldown);
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

    for (ACharacterBase* Target : HitActors)
    {
        if (!Target)
        {
            continue;
        }

        for (const FAbilityStatusEffect& Effect : AbilityDefinition->OnHitStatusEffects)
        {
            ApplyStatusEffect(SourceCharacter, Target, Effect);
        }
    }
}

void UBattleGameplayAbility::ApplySelfStatusEffects(ACharacterBase* SourceCharacter)
{
    if (!AbilityDefinition || AbilityDefinition->SelfStatusEffects.IsEmpty())
    {
        return;
    }

    for (const FAbilityStatusEffect& Effect : AbilityDefinition->SelfStatusEffects)
    {
        ApplyStatusEffect(SourceCharacter, SourceCharacter, Effect);
    }
}

bool UBattleGameplayAbility::HasSufficientAP(const FActionContext& Context) const
{
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
            TEXT("[BattleGameplayAbility] HasSufficientAP: %s has %d AP, needs %d — blocked"),
            *GetNameSafe(Source), UnitState->CurrentActionPoints, Cost);
    }
 
    return bSufficient;
}

void UBattleGameplayAbility::ApplyStatusEffect(
    ACharacterBase* SourceCharacter,
    ACharacterBase* TargetCharacter,
    const FAbilityStatusEffect& Effect
)
{
    if (!TargetCharacter || !Effect.StatusTag.IsValid())
    {
        return;
    }

    // Add the tag to the ASC so GAS-based systems (ActivationBlockedTags, etc.) react
    if (UAbilitySystemComponent* ASC = TargetCharacter->GetAbilitySystemComponent())
    {
        ASC->AddLooseGameplayTags(FGameplayTagContainer(Effect.StatusTag));
    }

    // Route through the event bus: updates BattleState, spawns floating text, broadcasts delegates
    UBattleModeComponent* BattleMode = SourceCharacter
        ? SourceCharacter->GetBattleModeComponent()
        : nullptr;

    if (UCombatEventBus* Bus = BattleMode ? BattleMode->GetCombatEventBus() : nullptr)
    {
        Bus->NotifyStatusApplied(TargetCharacter, Effect.StatusTag, Effect.DisplayLabel, Effect.Icon);

        UE_LOG(LogNevergone, Log,
            TEXT("[BattleGameplayAbility] Status '%s' applied to %s by %s"),
            *Effect.StatusTag.ToString(),
            *GetNameSafe(TargetCharacter),
            *GetNameSafe(SourceCharacter));
    }
    else
    {
        UE_LOG(LogNevergone, Warning,
            TEXT("[BattleGameplayAbility] ApplyStatusEffect: CombatEventBus not found — "
                 "status '%s' added to ASC only, BattleState not notified"),
            *Effect.StatusTag.ToString());
    }
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
    RemainingCooldownTurns = AbilityDefinition->CooldownTurns;

    ASC->AddLooseGameplayTags(FGameplayTagContainer(TAG_State_AbilityOnCooldown));

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

    RemainingCooldownTurns--;

    UE_LOG(LogNevergone, Log,
        TEXT("[BattleGameplayAbility] %s cooldown tick for %s — %d turns remaining"),
        *GetName(), *GetNameSafe(CooldownOwner), RemainingCooldownTurns);

    if (RemainingCooldownTurns <= 0)
    {
        ClearCooldown();
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
        if (UAbilitySystemComponent* ASC = CooldownOwner->GetAbilitySystemComponent())
        {
            ASC->RemoveLooseGameplayTags(FGameplayTagContainer(TAG_State_AbilityOnCooldown));

            UE_LOG(LogNevergone, Log,
                TEXT("[BattleGameplayAbility] %s cooldown expired for %s — ability available again"),
                *GetName(), *GetNameSafe(CooldownOwner));
        }
    }

    CooldownOwner = nullptr;
    RemainingCooldownTurns = 0;
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