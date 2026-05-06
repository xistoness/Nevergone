// Copyright Xyzto Works

#include "World/ItemUsageSubsystem.h"
#include "World/InventorySubsystem.h"
#include "Party/PartyManagerSubsystem.h"
#include "Data/ItemDefinition.h"
#include "Data/PartyData.h"
#include "GameInstance/GameContextManager.h"
#include "GameInstance/MyGameInstance.h"
#include "Types/LevelTypes.h"

// ===========================================================================
// Public API
// ===========================================================================

EItemUseResult UItemUsageSubsystem::CanUseItem(const FItemUseRequest& Request) const
{
    // 1. Item validity
    UItemDefinition* Def = Request.ItemDef;
    if (!Def)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[ItemUsage] CanUseItem: null ItemDef."));
        return EItemUseResult::FailedNullItem;
    }
    if (Def->Category != EItemCategory::Consumable)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[ItemUsage] CanUseItem: '%s' is not a consumable."),
            *Def->DisplayName.ToString());
        return EItemUseResult::FailedNotConsumable;
    }
    if (Def->ConsumableEffects.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ItemUsage] CanUseItem: '%s' has no consumable effects defined."),
            *Def->DisplayName.ToString());
        return EItemUseResult::FailedNoEffects;
    }

    // 2. Source has the item
    const EItemUseResult SourceResult = ValidateSourceHasItem(Request);
    if (SourceResult != EItemUseResult::Success)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[ItemUsage] CanUseItem: '%s' not present at source (code=%d)."),
            *Def->DisplayName.ToString(), (int32)SourceResult);
        return SourceResult;
    }

    // 3. Target party member exists in the runtime cache
    UPartyManagerSubsystem* PartyMgr = GetPartyManager();
    const FPartyMemberData* Target = PartyMgr ? PartyMgr->FindMemberMutable(Request.TargetCharacterID) : nullptr;
    if (!Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ItemUsage] CanUseItem: target party member not found (ID=%s)."),
            *Request.TargetCharacterID.ToString());
        return EItemUseResult::FailedInvalidTarget;
    }

    // 4. Effect applicability — categorize what the item is capable of in
    //    this context, so we can return a specific error code instead of the
    //    generic FailedNoApplicableEffects whenever possible.
    const bool bInCombat = IsInCombat();

    int32 ApplicableCount      = 0;
    bool  bHasReviveEffect     = false;
    bool  bHasNonReviveEffect  = false;
    bool  bOnlyHealsAndRevives = true;
    bool  bHasHealEffect       = false;

    for (const FConsumableEffect& Effect : Def->ConsumableEffects)
    {
        const bool bIsRevive = (Effect.EffectType == EConsumableEffectType::ReviveUnit);
        const bool bIsHeal   = (Effect.EffectType == EConsumableEffectType::HealHP);

        if (bIsRevive) { bHasReviveEffect = true; }
        else           { bHasNonReviveEffect = true; }
        if (bIsHeal)   { bHasHealEffect = true; }

        if (!bIsRevive && !bIsHeal)
        {
            bOnlyHealsAndRevives = false;
        }

        if (IsEffectApplicable(Effect, *Target, bInCombat))
        {
            ++ApplicableCount;
        }
    }

    // 5. Specific waste rules — surface clear error codes for the obvious cases.
    if (!Target->bIsAlive)
    {
        // Dead target: only revive items make sense.
        if (!bHasReviveEffect)
        {
            return EItemUseResult::FailedTargetDead;
        }
    }
    else
    {
        // Alive target: a pure-revive item is wasted.
        if (bHasReviveEffect && !bHasNonReviveEffect)
        {
            return EItemUseResult::FailedTargetAlreadyAlive;
        }
        // A pure-heal (or heal+revive) item used on a full-HP alive target is wasted.
        // Note: revive effects don't contribute on an alive target, so heal+revive
        // items still count as "only heals" for this check.
        if (bHasHealEffect && bOnlyHealsAndRevives && Target->CurrentHP >= Target->MaxHP)
        {
            return EItemUseResult::FailedAlreadyAtFullHP;
        }
    }

    // 6. Catch-all: at least one effect must actually apply.
    if (ApplicableCount == 0)
    {
        return EItemUseResult::FailedNoApplicableEffects;
    }

    return EItemUseResult::Success;
}

EItemUseResult UItemUsageSubsystem::TryUseItem(const FItemUseRequest& Request)
{
    // Validate up front. On any failure, no state changes and no item is consumed.
    const EItemUseResult Validation = CanUseItem(Request);
    if (Validation != EItemUseResult::Success)
    {
        UE_LOG(LogTemp, Log, TEXT("[ItemUsage] TryUseItem blocked: result=%d item='%s'."),
            (int32)Validation,
            Request.ItemDef ? *Request.ItemDef->DisplayName.ToString() : TEXT("null"));
        return Validation;
    }

    UItemDefinition* Def       = Request.ItemDef;
    const bool       bInCombat = IsInCombat();

    UE_LOG(LogTemp, Log, TEXT("[ItemUsage] TryUseItem: '%s' on TargetID=%s (combat=%s, source=%d)."),
        *Def->DisplayName.ToString(),
        *Request.TargetCharacterID.ToString(),
        bInCombat ? TEXT("true") : TEXT("false"),
        (int32)Request.Source);

    int32 EffectsApplied = 0;

    if (bInCombat)
    {
        // In-combat path is stubbed. The combat skill caller will be responsible
        // for resolving the target ACharacterBase from the BattleState before
        // calling — when implementing that skill, plumb it through Request.Caster
        // and a separate Target actor lookup here.
        for (const FConsumableEffect& Effect : Def->ConsumableEffects)
        {
            ACharacterBase* TargetActor = nullptr; // TODO: resolve from BattleState by CharacterID
            if (ApplyEffect_InCombat(Effect, TargetActor, Request.Caster))
            {
                ++EffectsApplied;
            }
        }
    }
    else
    {
        // Out-of-combat path — route ALL mutations through PartyManagerSubsystem
        // so its runtime cache (the source of truth read by PartyManagerWidget,
        // PartyMemberEntryWidget, etc.) stays in sync. Writing directly to
        // UMyGameInstance::PartyData here was the source of an earlier bug:
        // the heal landed on the GI copy but the subsystem's cache kept the
        // pre-heal HP, so the UI showed the old value until the next
        // SyncFromGameInstance.
        UPartyManagerSubsystem* PartyMgr = GetPartyManager();
        FPartyMemberData* Target = PartyMgr ? PartyMgr->FindMemberMutable(Request.TargetCharacterID) : nullptr;
        if (!Target)
        {
            // Defensive: validation already passed, but state could change asynchronously.
            UE_LOG(LogTemp, Warning,
                TEXT("[ItemUsage] TryUseItem: target disappeared between validation and apply (ID=%s)."),
                *Request.TargetCharacterID.ToString());
            return EItemUseResult::FailedInvalidTarget;
        }

        LogTargetState(*Target, TEXT("PRE-USE"));

        for (const FConsumableEffect& Effect : Def->ConsumableEffects)
        {
            if (!IsEffectApplicable(Effect, *Target, /*bInCombat=*/false))
            {
                UE_LOG(LogTemp, Verbose,
                    TEXT("[ItemUsage]   skipping non-applicable effect type=%d."),
                    (int32)Effect.EffectType);
                continue;
            }
            if (ApplyEffect_OutOfCombat(Effect, *Target))
            {
                ++EffectsApplied;
            }
        }

        LogTargetState(*Target, TEXT("POST-USE"));
    }

    if (EffectsApplied == 0)
    {
        // Defensive: validation said at least one applicable effect existed.
        // Hitting this path means the apply layer silently no-op'd — bug worth surfacing.
        UE_LOG(LogTemp, Warning,
            TEXT("[ItemUsage] TryUseItem: no effects applied for '%s' despite passing validation."),
            *Def->DisplayName.ToString());
        return EItemUseResult::FailedNoApplicableEffects;
    }

    // Consume one unit from the source.
    switch (Request.Source)
    {
    case EItemUseSource::Inventory:
        ConsumeFromInventory(Def);
        break;

    case EItemUseSource::EquippedConsumable:
        if (UPartyManagerSubsystem* PartyMgr = GetPartyManager())
        {
            if (FPartyMemberData* Target = PartyMgr->FindMemberMutable(Request.TargetCharacterID))
            {
                ConsumeFromEquippedSlot(*Target, Request.EquippedSlotIndex);
            }
        }
        break;
    }

    // Propagate the FPartyMemberData mutations from the cache to the
    // GameInstance and notify subscribed widgets to refresh.
    // Skip when in combat: BattleUnitState is the authority during a session
    // and FPartyMemberData is rebuilt from it via WriteBackBattleResults.
    if (!bInCombat)
    {
        if (UPartyManagerSubsystem* PartyMgr = GetPartyManager())
        {
            PartyMgr->NotifyMemberMutated();
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("[ItemUsage] TryUseItem: '%s' applied %d effect(s) and consumed from source=%d."),
        *Def->DisplayName.ToString(), EffectsApplied, (int32)Request.Source);

    OnItemUsed.Broadcast(Request);
    return EItemUseResult::Success;
}

// ===========================================================================
// Context detection
// ===========================================================================

bool UItemUsageSubsystem::IsInCombat() const
{
    UMyGameInstance* GI = GetMyGameInstance();
    if (!GI) return false;

    UGameContextManager* CtxMgr = GI->GetSubsystem<UGameContextManager>();
    return CtxMgr && CtxMgr->GetCurrentState() == EGameContextState::Battle;
}

// ===========================================================================
// Validation helpers
// ===========================================================================

EItemUseResult UItemUsageSubsystem::ValidateSourceHasItem(const FItemUseRequest& Request) const
{
    UItemDefinition* Def = Request.ItemDef;

    switch (Request.Source)
    {
    case EItemUseSource::Inventory:
    {
        UMyGameInstance* GI = GetMyGameInstance();
        UInventorySubsystem* Inv = GI ? GI->GetSubsystem<UInventorySubsystem>() : nullptr;
        if (!Inv) return EItemUseResult::FailedItemNotFound;
        return Inv->HasItem(Def) ? EItemUseResult::Success : EItemUseResult::FailedItemNotFound;
    }

    case EItemUseSource::EquippedConsumable:
    {
        UPartyManagerSubsystem* PartyMgr = GetPartyManager();
        FPartyMemberData* Target = PartyMgr ? PartyMgr->FindMemberMutable(Request.TargetCharacterID) : nullptr;
        if (!Target) return EItemUseResult::FailedInvalidTarget;

        if (!Target->EquipmentLoadout.CombatConsumables.IsValidIndex(Request.EquippedSlotIndex))
        {
            return EItemUseResult::FailedItemNotFound;
        }

        const TSoftObjectPtr<UItemDefinition>& SlotItem =
            Target->EquipmentLoadout.CombatConsumables[Request.EquippedSlotIndex];

        if (SlotItem.IsNull() || SlotItem.LoadSynchronous() != Def)
        {
            return EItemUseResult::FailedItemNotFound;
        }
        return EItemUseResult::Success;
    }
    }
    return EItemUseResult::FailedItemNotFound;
}

bool UItemUsageSubsystem::IsEffectApplicable(
    const FConsumableEffect& Effect,
    const FPartyMemberData&  Target,
    bool                     bInCombat) const
{
    switch (Effect.EffectType)
    {
    case EConsumableEffectType::HealHP:
        // Need an alive target whose HP is below max.
        return Target.bIsAlive && Target.CurrentHP < Target.MaxHP;

    case EConsumableEffectType::ReviveUnit:
        return !Target.bIsAlive;

    case EConsumableEffectType::RestoreAP:
        // AP exists only during combat.
        return bInCombat && Target.bIsAlive;

    case EConsumableEffectType::ApplyStatus:
    case EConsumableEffectType::RemoveStatus:
        // Status effects live on FBattleUnitState — combat-only.
        return bInCombat && Target.bIsAlive;
    }
    return false;
}

// ===========================================================================
// Out-of-combat effect application
// ===========================================================================

bool UItemUsageSubsystem::ApplyEffect_OutOfCombat(const FConsumableEffect& Effect, FPartyMemberData& Target)
{
    switch (Effect.EffectType)
    {
    case EConsumableEffectType::HealHP:
    {
        const float Before = Target.CurrentHP;
        Target.CurrentHP   = FMath::Min<float>(Target.CurrentHP + Effect.FlatValue, Target.MaxHP);
        const float Delta  = Target.CurrentHP - Before;

        UE_LOG(LogTemp, Log, TEXT("[ItemUsage]   HealHP: %s %.0f -> %.0f (+%.0f)."),
            *Target.DisplayName.ToString(), Before, Target.CurrentHP, Delta);

        return Delta > 0.f;
    }

    case EConsumableEffectType::ReviveUnit:
    {
        if (Target.bIsAlive)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[ItemUsage]   ReviveUnit: %s already alive, skipping."),
                *Target.DisplayName.ToString());
            return false;
        }

        // Clamp revive HP to [1, MaxHP] so the unit always comes back alive,
        // even if the design data accidentally has a 0 FlatValue.
        Target.bIsAlive  = true;
        Target.CurrentHP = FMath::Max<float>(1.f, FMath::Min<float>((float)Effect.FlatValue, Target.MaxHP));

        UE_LOG(LogTemp, Log, TEXT("[ItemUsage]   ReviveUnit: %s revived with HP=%.0f."),
            *Target.DisplayName.ToString(), Target.CurrentHP);

        return true;
    }

    case EConsumableEffectType::RestoreAP:
    case EConsumableEffectType::ApplyStatus:
    case EConsumableEffectType::RemoveStatus:
        // Skipped silently — IsEffectApplicable already filtered these out
        // before this function is called.
        return false;
    }
    return false;
}

// ===========================================================================
// In-combat effect application — stub
// ===========================================================================

bool UItemUsageSubsystem::ApplyEffect_InCombat(
    const FConsumableEffect& Effect,
    ACharacterBase*          TargetActor,
    ACharacterBase*          Caster)
{
    // TODO: combat path — to be implemented alongside the Use Item battle skill.
    //
    //   HealHP        -> CombatEventBus::NotifyHealApplied(Caster, TargetActor, Effect.FlatValue)
    //   RestoreAP     -> TargetActor->GetUnitStats()->SetCurrentActionPoints(Cur + Effect.FlatValue)
    //                    plus BattleState sync
    //   ApplyStatus   -> StatusEffectManager::ApplyStatusEffect(Caster, TargetActor, Effect.StatusEffect.LoadSynchronous())
    //   RemoveStatus  -> StatusEffectManager::RemoveStatusEffect(TargetActor, Status->StatusTag)
    //   ReviveUnit    -> resurrect actor: undo PlayDeathVisual, restore HP, broadcast OnHealApplied
    //
    // Routing to CombatEventBus / StatusEffectManager will require pulling them
    // from the active UCombatManager (UGameContextManager::GetActiveCombatManager()).
    UE_LOG(LogTemp, Warning,
        TEXT("[ItemUsage] ApplyEffect_InCombat: not yet implemented (effect type=%d)."),
        (int32)Effect.EffectType);
    return false;
}

// ===========================================================================
// Source consumption
// ===========================================================================

void UItemUsageSubsystem::ConsumeFromInventory(UItemDefinition* ItemDef)
{
    UMyGameInstance* GI = GetMyGameInstance();
    UInventorySubsystem* Inv = GI ? GI->GetSubsystem<UInventorySubsystem>() : nullptr;
    if (!Inv)
    {
        UE_LOG(LogTemp, Error, TEXT("[ItemUsage] ConsumeFromInventory: InventorySubsystem missing."));
        return;
    }
    Inv->RemoveItem(ItemDef, 1);
}

void UItemUsageSubsystem::ConsumeFromEquippedSlot(FPartyMemberData& Target, int32 SlotIndex)
{
    if (!Target.EquipmentLoadout.CombatConsumables.IsValidIndex(SlotIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("[ItemUsage] ConsumeFromEquippedSlot: invalid slot %d on %s."),
            SlotIndex, *Target.DisplayName.ToString());
        return;
    }

    Target.EquipmentLoadout.CombatConsumables[SlotIndex].Reset();

    UE_LOG(LogTemp, Log, TEXT("[ItemUsage] Cleared equipped consumable slot %d on %s."),
        SlotIndex, *Target.DisplayName.ToString());
}

// ===========================================================================
// Lookup
// ===========================================================================

UPartyManagerSubsystem* UItemUsageSubsystem::GetPartyManager() const
{
    UMyGameInstance* GI = GetMyGameInstance();
    return GI ? GI->GetSubsystem<UPartyManagerSubsystem>() : nullptr;
}

void UItemUsageSubsystem::LogTargetState(const FPartyMemberData& Target, const TCHAR* Phase) const
{
    UE_LOG(LogTemp, Log,
        TEXT("[ItemUsage] %s state — %s (ID=%s) HP=%.0f/%.0f alive=%s"),
        Phase,
        *Target.DisplayName.ToString(),
        *Target.CharacterID.ToString(),
        Target.CurrentHP, Target.MaxHP,
        Target.bIsAlive ? TEXT("true") : TEXT("false"));
}

UMyGameInstance* UItemUsageSubsystem::GetMyGameInstance() const
{
    return Cast<UMyGameInstance>(GetOuter());
}
