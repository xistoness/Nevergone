// Copyright Xyzto Works

#include "Widgets/Combat/ActionHotbar.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "ActorComponents/BattleModeComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "Data/AbilityDefinition.h"
#include "GameMode/CombatManager.h"
#include "Types/CharacterTypes.h"
#include "Widgets/Combat/ActionSlot.h"
#include "Nevergone.h"
#include "AbilitySystemComponent.h"
#include "ActorComponents/MyAbilitySystemComponent.h"
#include "GameMode/TurnManager.h"

// ---- Public API ------------------------------------------------------------

void UActionHotbar::InitializeWithCombatManager(UCombatManager* InCombatManager)
{
    if (!InCombatManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ActionHotbar] InitializeWithCombatManager: received null CombatManager."));
        return;
    }

    UnbindFromCombatManager();

    TrackedCombatManager = InCombatManager;

    InCombatManager->OnActiveUnitChanged.AddDynamic(this, &UActionHotbar::HandleActiveUnitChanged);
    InCombatManager->OnEnemyTurnBegan.AddDynamic(this, &UActionHotbar::HandleEnemyTurnBegan);

    // Subscribe to turn changes to update cooldown counts each turn
    if (UTurnManager* TM = InCombatManager->GetTurnManager())
    {
        TurnStateHandle = TM->OnTurnStateChanged.AddUObject(
            this, &UActionHotbar::HandleTurnStateChangedForCooldown);
    }

    UE_LOG(LogTemp, Log, TEXT("[ActionHotbar] InitializeWithCombatManager: bound to CombatManager."));

    HideHotbar();
}

void UActionHotbar::RefreshSelection(int32 AbilityIndex)
{
    if (bIsLocked)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("[ActionHotbar] RefreshSelection: hotbar locked during execution — ignoring index %d."), AbilityIndex);
        return;
    }

    if (CurrentSelectedIndex == AbilityIndex)
    {
        return;
    }

    UE_LOG(LogTemp, Log,
        TEXT("[ActionHotbar] RefreshSelection: %d → %d"), CurrentSelectedIndex, AbilityIndex);

    if (Slots.IsValidIndex(CurrentSelectedIndex))
    {
        Slots[CurrentSelectedIndex]->SetSelected(false);
    }

    if (Slots.IsValidIndex(AbilityIndex))
    {
        Slots[AbilityIndex]->SetSelected(true);
    }

    CurrentSelectedIndex = AbilityIndex;

    // Clear the AP cost display when the player switches ability —
    // it will be repopulated on the next hover via OnPreviewUpdated.
    ClearAPDisplay();
}

void UActionHotbar::ClearHotbar()
{
    UnbindFromCombatManager();
    UnbindFromCurrentUnit();
    ClearSlots();
    ClearAPDisplay();
    bIsLocked = false;
    CurrentSelectedIndex = INDEX_NONE;

    UE_LOG(LogTemp, Log, TEXT("[ActionHotbar] ClearHotbar: fully reset."));
}

// ---- Protected -------------------------------------------------------------

void UActionHotbar::NativeDestruct()
{
    UnbindFromCombatManager();
    UnbindFromCurrentUnit();
    Super::NativeDestruct();
}

// ---- Private — unit management ---------------------------------------------

void UActionHotbar::ShowForUnit(ACharacterBase* Unit)
{
    UnbindFromCurrentUnit();
    ClearSlots();
    ClearAPDisplay();
    CurrentSelectedIndex = INDEX_NONE;
    bIsLocked = false;

    if (!Unit)
    {
        HideHotbar();
        return;
    }

    UBattleModeComponent* BattleMode = Unit->GetBattleModeComponent();
    if (!BattleMode)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ActionHotbar] ShowForUnit: unit '%s' has no BattleModeComponent."), *GetNameSafe(Unit));
        HideHotbar();
        return;
    }

    if (!ActionSlotClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[ActionHotbar] ShowForUnit: ActionSlotClass is not set — assign it in the Blueprint defaults."));
        return;
    }

    if (!SlotsBox)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[ActionHotbar] ShowForUnit: SlotsBox binding is missing — check the widget Blueprint."));
        return;
    }

    TrackedUnit       = Unit;
    TrackedBattleMode = BattleMode;

    BattleMode->OnActionUseStarted.AddDynamic(this, &UActionHotbar::HandleActionStarted);
    BattleMode->OnActionUseFinished.AddDynamic(this, &UActionHotbar::HandleActionFinished);
    BattleMode->OnAbilitySelectionChanged.AddDynamic(this, &UActionHotbar::HandleAbilitySelectionChanged);
    BattleMode->OnPreviewUpdated.AddDynamic(this, &UActionHotbar::HandlePreviewUpdated);

    const TArray<FUnitAbilityEntry>& Abilities = BattleMode->GetGrantedBattleAbilities();

    UE_LOG(LogTemp, Log,
        TEXT("[ActionHotbar] ShowForUnit: building %d slots for unit '%s'."),
        Abilities.Num(), *GetNameSafe(Unit));

    for (int32 Index = 0; Index < Abilities.Num(); ++Index)
    {
        const FUnitAbilityEntry& Entry = Abilities[Index];

        UActionSlot* NewSlot = CreateWidget<UActionSlot>(this, ActionSlotClass);
        if (!NewSlot)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[ActionHotbar] ShowForUnit: failed to create ActionSlot at index %d."), Index);
            continue;
        }

        NewSlot->SetAbility(Entry.Definition);

        UHorizontalBoxSlot* BoxSlot = SlotsBox->AddChildToHorizontalBox(NewSlot);
        if (BoxSlot)
        {
            FSlateChildSize AutoSize;
            AutoSize.SizeRule = ESlateSizeRule::Automatic;
            BoxSlot->SetSize(AutoSize);

            BoxSlot->SetPadding(FMargin(8.f, 0.f, 8.f, 0.f));
        }

        Slots.Add(NewSlot);
    }

    const FActionContext& CurrentCtx = BattleMode->GetCurrentContext();
    int32 InitialIndex = INDEX_NONE;

    for (int32 Index = 0; Index < Abilities.Num(); ++Index)
    {
        if (Abilities[Index].Definition &&
            Abilities[Index].Definition->AbilityClass == CurrentCtx.AbilityClass)
        {
            InitialIndex = Index;
            break;
        }
    }

    if (InitialIndex == INDEX_NONE && Slots.Num() > 0)
    {
        InitialIndex = 0;
    }

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    RefreshSelection(InitialIndex);

    // Apply cooldown overlays to any abilities already on cooldown
    RefreshCooldownStates();
}

void UActionHotbar::HideHotbar()
{
    SetVisibility(ESlateVisibility::Collapsed);
    UE_LOG(LogTemp, Log, TEXT("[ActionHotbar] HideHotbar: hotbar is now hidden."));
}

void UActionHotbar::ClearSlots()
{
    if (SlotsBox)
    {
        SlotsBox->ClearChildren();
    }
    Slots.Empty();
}

void UActionHotbar::UnbindFromCurrentUnit()
{
    if (TrackedBattleMode.IsValid())
    {
        TrackedBattleMode->OnActionUseStarted.RemoveDynamic(this, &UActionHotbar::HandleActionStarted);
        TrackedBattleMode->OnActionUseFinished.RemoveDynamic(this, &UActionHotbar::HandleActionFinished);
        TrackedBattleMode->OnAbilitySelectionChanged.RemoveDynamic(this, &UActionHotbar::HandleAbilitySelectionChanged);
        TrackedBattleMode->OnPreviewUpdated.RemoveDynamic(this, &UActionHotbar::HandlePreviewUpdated);

        UE_LOG(LogTemp, Log,
            TEXT("[ActionHotbar] UnbindFromCurrentUnit: unbound from '%s'."), *GetNameSafe(TrackedUnit.Get()));
    }

    TrackedUnit.Reset();
    TrackedBattleMode.Reset();
}

void UActionHotbar::UnbindFromCombatManager()
{
    if (TrackedCombatManager.IsValid())
    {
        TrackedCombatManager->OnActiveUnitChanged.RemoveDynamic(this, &UActionHotbar::HandleActiveUnitChanged);
        TrackedCombatManager->OnEnemyTurnBegan.RemoveDynamic(this, &UActionHotbar::HandleEnemyTurnBegan);

        if (UTurnManager* TM = TrackedCombatManager->GetTurnManager())
        {
            if (TurnStateHandle.IsValid())
            {
                TM->OnTurnStateChanged.Remove(TurnStateHandle);
                TurnStateHandle.Reset();
            }
        }

        UE_LOG(LogTemp, Log, TEXT("[ActionHotbar] UnbindFromCombatManager: unbound from CombatManager."));
    }

    TrackedCombatManager.Reset();
}

// ---- Private — AP display --------------------------------------------------

void UActionHotbar::RefreshAPDisplay(int32 Cost, int32 Remaining, bool bPreviewIsValid)
{
    if (APCost)
    {
        // Show "--" when hovering an invalid target so the player knows the
        // action won't resolve, rather than showing a misleading 0.
        const FText CostText = bPreviewIsValid
            ? FText::AsNumber(Cost)
            : FText::FromString(TEXT("--"));

        APCost->SetText(CostText);
    }

    if (RemainingAP)
    {
        RemainingAP->SetText(FText::AsNumber(Remaining));
    }
}

void UActionHotbar::ClearAPDisplay()
{
    if (APCost)
    {
        APCost->SetText(FText::FromString(TEXT("--")));
    }

    if (RemainingAP)
    {
        RemainingAP->SetText(FText::FromString(TEXT("--")));
    }
}

// ---- Private — delegate handlers -------------------------------------------

void UActionHotbar::HandleActiveUnitChanged(ACharacterBase* NewActiveUnit)
{
    if (NewActiveUnit)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[ActionHotbar] HandleActiveUnitChanged: showing hotbar for '%s'."), *GetNameSafe(NewActiveUnit));
        ShowForUnit(NewActiveUnit);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[ActionHotbar] HandleActiveUnitChanged: no unit — hiding hotbar."));
        UnbindFromCurrentUnit();
        ClearSlots();
        ClearAPDisplay();
        HideHotbar();
    }
}

void UActionHotbar::HandleEnemyTurnBegan()
{
    UE_LOG(LogTemp, Log, TEXT("[ActionHotbar] HandleEnemyTurnBegan: hiding hotbar for enemy turn."));
    UnbindFromCurrentUnit();
    ClearSlots();
    ClearAPDisplay();
    HideHotbar();
}

void UActionHotbar::HandleActionStarted(ACharacterBase* ActingUnit)
{
    bIsLocked = true;
    UE_LOG(LogTemp, Log,
        TEXT("[ActionHotbar] HandleActionStarted: locking hotbar for '%s'."), *GetNameSafe(ActingUnit));
}

void UActionHotbar::HandleActionFinished(ACharacterBase* ActingUnit)
{
    bIsLocked = false;

    UE_LOG(LogTemp, Log,
        TEXT("[ActionHotbar] HandleActionFinished: unlocking hotbar for '%s'."), *GetNameSafe(ActingUnit));

    // Re-sync slot highlight after execution — the active ability may have
    // switched back to the default inside BattleModeComponent.
    if (TrackedBattleMode.IsValid())
    {
        const TArray<FUnitAbilityEntry>& Abilities = TrackedBattleMode->GetGrantedBattleAbilities();
        const FActionContext& Ctx = TrackedBattleMode->GetCurrentContext();

        for (int32 Index = 0; Index < Abilities.Num(); ++Index)
        {
            if (Abilities[Index].Definition &&
                Abilities[Index].Definition->AbilityClass == Ctx.AbilityClass)
            {
                CurrentSelectedIndex = INDEX_NONE;
                RefreshSelection(Index);
                break;
            }
        }
    }

    // Clear AP cost — the player hasn't hovered a new target yet after the action.
    ClearAPDisplay();

    // Update cooldown overlays — the ability that just ran may have started a cooldown
    RefreshCooldownStates();
}

void UActionHotbar::HandleAbilitySelectionChanged(int32 NewIndex)
{
    UE_LOG(LogTemp, Log,
        TEXT("[ActionHotbar] HandleAbilitySelectionChanged: new index = %d"), NewIndex);
    RefreshSelection(NewIndex);
}

void UActionHotbar::HandlePreviewUpdated(int32 Cost, int32 Remaining, bool bPreviewIsValid)
{
    RefreshAPDisplay(Cost, Remaining, bPreviewIsValid);
}
// ---- Cooldown display ------------------------------------------------------

void UActionHotbar::RefreshCooldownStates()
{
    if (!TrackedUnit.IsValid() || !TrackedBattleMode.IsValid()) { return; }

    const TArray<FUnitAbilityEntry>& Abilities = TrackedBattleMode->GetGrantedBattleAbilities();

    for (int32 Index = 0; Index < Abilities.Num(); ++Index)
    {
        if (!Slots.IsValidIndex(Index)) { break; }

        const UAbilityDefinition* Def = Abilities[Index].Definition;

        // Query BattleModeComponent directly -- per-definition cooldown,
        // Thunder and HealingRain have independent counters.
        if (Def && TrackedBattleMode->IsDefinitionOnCooldown(Def))
        {
            Slots[Index]->SetCooldown(true, TrackedBattleMode->GetDefinitionCooldownTurns(Def));
        }
        else
        {
            Slots[Index]->SetCooldown(false);
        }
    }
}

void UActionHotbar::HandleTurnStateChangedForCooldown(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase)
{
    // Refresh on AwaitingOrders — this fires once per turn start, after
    // BattleGameplayAbility::OnTurnStateChanged has already decremented the counters.
    if (NewPhase == EBattleTurnPhase::AwaitingOrders)
    {
        RefreshCooldownStates();
    }
}