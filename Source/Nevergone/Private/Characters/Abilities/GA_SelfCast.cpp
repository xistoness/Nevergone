// Copyright Xyzto Works

#include "Characters/Abilities/GA_SelfCast.h"

#include "ActorComponents/BattleModeComponent.h"
#include "Characters/CharacterBase.h"
#include "Data/AbilityDefinition.h"
#include "GameMode/Combat/BattleState.h"
#include "Nevergone.h"
#include "Types/BattleTypes.h"
#include "Characters/Abilities/AbilityPreview/Preview_SelfCast.h"

UGA_SelfCast::UGA_SelfCast()
{
    // Highlight the caster's own tile — no grid targeting cursor needed.
    PreviewRendererClass = UPreview_SelfCast::StaticClass();
}

EBattleAbilitySelectionMode UGA_SelfCast::GetSelectionMode() const
{
    // Self-cast abilities bypass the targeting phase entirely.
    // The player selects the ability from the hotbar and confirms immediately.
    return EBattleAbilitySelectionMode::SingleConfirm;
}

FActionResult UGA_SelfCast::BuildPreview(const FActionContext& Context) const
{
    FActionResult Result;
    BuildSelfCastResult(Context, Result);
    return Result;
}

FActionResult UGA_SelfCast::BuildExecution(const FActionContext& Context) const
{
    FActionResult Result;
    BuildSelfCastResult(Context, Result);
    return Result;
}

bool UGA_SelfCast::BuildSelfCastResult(const FActionContext& Context, FActionResult& OutResult) const
{
    OutResult = FActionResult();

    // Self-cast abilities only require at least 1 AP to activate.
    // AP drain (if any) is handled by DrainActionPoints in AbilityDefinition->SelfStatusEffects,
    // not by ActionPointsCost — so the unit can always use this ability while it has any AP left.
    const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(Context.SourceActor);
    if (!SourceCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_SelfCast] BuildSelfCastResult: no source character in context"));
        return false;
    }

    if (!AbilityDefinition)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[GA_SelfCast] BuildSelfCastResult: AbilityDefinition is null on %s"),
            *GetNameSafe(SourceCharacter));
        return false;
    }

    // Cooldown is still enforced — self-cast abilities can have cooldowns.
    if (IsOnCooldown())
    {
        UE_LOG(LogTemp, Log,
            TEXT("[GA_SelfCast] BuildSelfCastResult: '%s' is on cooldown for %s"),
            *AbilityDefinition->DisplayName.ToString(), *GetNameSafe(SourceCharacter));
        return false;
    }

    const UBattleModeComponent* BattleMode     = SourceCharacter->GetBattleModeComponent();
    const UBattleState*         BattleStateRef = BattleMode ? BattleMode->GetBattleState() : nullptr;
    const FBattleUnitState*     UnitState      = BattleStateRef ? BattleStateRef->FindUnitState(const_cast<ACharacterBase*>(SourceCharacter)) : nullptr;

    if (!UnitState || UnitState->CurrentActionPoints < 1)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[GA_SelfCast] BuildSelfCastResult: %s has no AP remaining — blocked"),
            *GetNameSafe(SourceCharacter));
        return false;
    }

    // Self-cast is always valid as long as AP >= 1.
    // ActionPointsCost is intentionally 0 here — AP drain is handled by
    // the DrainActionPoints SelfStatusEffect on the AbilityDefinition,
    // so the ability fires regardless of how many AP the unit has left.
    OutResult.bIsValid          = true;
    OutResult.bRequiresMovement = false;
    OutResult.ActionPointsCost  = 0;

    UE_LOG(LogTemp, Verbose,
        TEXT("[GA_SelfCast] BuildSelfCastResult: '%s' valid for %s — AP will be drained via SelfStatusEffect"),
        *AbilityDefinition->DisplayName.ToString(),
        *GetNameSafe(SourceCharacter));

    return true;
}

void UGA_SelfCast::ActivateAbility(
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
    const FActionResult  ExecutionResult = BuildExecution(Context);

    if (!ExecutionResult.bIsValid)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[GA_SelfCast] ActivateAbility: execution result invalid for %s — cancelling"),
            *GetNameSafe(SourceCharacter));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    CachedCharacter        = SourceCharacter;

    // Play cast SFX if one is assigned — SkipTurn will typically leave this null.
    PlayCastSFX(SourceCharacter);

    UE_LOG(LogTemp, Log,
        TEXT("[GA_SelfCast] ActivateAbility: '%s' activated by %s"),
        *GetNameSafe(AbilityDefinition),
        *GetNameSafe(SourceCharacter));

    FinalizeAbilityExecution();
}

void UGA_SelfCast::ApplyAbilityCompletionEffects()
{
    Super::ApplyAbilityCompletionEffects();

    if (!CachedCharacter)
    {
        return;
    }

    UBattleModeComponent* BattleMode = CachedCharacter->GetBattleModeComponent();
    if (!BattleMode)
    {
        return;
    }

    // AP drain is handled by DrainActionPoints in AbilityDefinition->SelfStatusEffects.
    // No manual ConsumeActionPoints call here — ApplySelfStatusEffects below fires it.
    ApplySelfStatusEffects(CachedCharacter);

    TryStartCooldown(CachedCharacter);
}

void UGA_SelfCast::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled
)
{
    CachedCharacter = nullptr;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}