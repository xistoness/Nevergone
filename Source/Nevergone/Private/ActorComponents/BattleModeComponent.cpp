// Copyright Xyzto Works

#include "ActorComponents/BattleModeComponent.h"

#include "Abilities/GameplayAbility.h"
#include "ActorComponents/MyAbilitySystemComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "Characters/Abilities/BattleMovementAbility.h"
#include "Characters/Abilities/AbilityPreview/AbilityPreviewRenderer.h"
#include "Data/AbilityDefinition.h"
#include "Data/UnitDefinition.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "GameMode/Combat/Resolvers/ActionResolver.h"
#include "Level/GridManager.h"
#include "Nevergone.h"

UBattleModeComponent::UBattleModeComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UBattleModeComponent::SetCombatEventBus(UCombatEventBus* InBus)
{
    CombatEventBus = InBus;
}

void UBattleModeComponent::SetTurnManager(UTurnManager* InTurnManager)
{
    TurnManager = InTurnManager;
}

void UBattleModeComponent::SetBattleState(UBattleState* InBattleState)
{
    BattleState = InBattleState;
}

void UBattleModeComponent::SetStatusEffectManager(UStatusEffectManager* InManager)
{
    StatusEffectManager = InManager;
}

void UBattleModeComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

UUnitStatsComponent* UBattleModeComponent::GetUnitStats() const
{
    if (const ACharacterBase* Character = Cast<ACharacterBase>(GetOwner()))
    {
        return Character->GetUnitStats();
    }
    return nullptr;
}

void UBattleModeComponent::HandleUnitDeath(ACharacterBase* OwnerCharacter)
{
    FVector DeadLocation = OwnerCharacter->GetActorLocation();
    DeadLocation.Z -= 1000.f;
    OwnerCharacter->SetActorLocation(DeadLocation);
}

void UBattleModeComponent::EnterMode()
{
    Super::EnterMode();
    SetComponentTickEnabled(true);

    UE_LOG(LogTemp, Log, TEXT("[BattleModeComponent] Entering battle mode for %s"),
        *GetNameSafe(GetOwner()));

    ActionResolver = NewObject<UActionResolver>(this);

    ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
    if (!Character) { return; }

    UMyAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
    if (!ASC) { return; }

    if (UUnitStatsComponent* Stats = GetUnitStats())
    {
        Stats->OnUnitDeath.AddUObject(this, &UBattleModeComponent::HandleUnitDeath);
    }

    ASC->InitAbilityActorInfo(Character, Character);

    InitializeAbilitiesFromDefinition();
    SelectDefaultAbility();
}

void UBattleModeComponent::ExitMode()
{
    Super::ExitMode();
    SetComponentTickEnabled(false);

    if (UUnitStatsComponent* Stats = GetUnitStats())
    {
        Stats->OnUnitDeath.RemoveAll(this);
    }
}

void UBattleModeComponent::InitializeAbilitiesFromDefinition()
{
    GrantedBattleAbilities.Reset();
    SelectedAbilityIndex = INDEX_NONE;

    UUnitStatsComponent* Stats = GetUnitStats();
    if (!Stats) { return; }

    const UUnitDefinition* UnitDef = Stats->GetDefinition();
    if (!UnitDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent] UnitDefinition is null on %s"),
            *GetNameSafe(GetOwner()));
        return;
    }

    for (const FUnitAbilityEntry& Entry : UnitDef->BattleAbilities)
    {
        if (!Entry.Definition || !Entry.Definition->AbilityClass)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[BattleModeComponent] Skipping ability entry with null Definition or AbilityClass on %s"),
                *GetNameSafe(GetOwner()));
            continue;
        }

        GrantedBattleAbilities.Add(Entry);

        if (Entry.bGrantedAtBattleStart)
        {
            EnsureAbilityGranted(Entry.Definition->AbilityClass);

            // Inject AbilityDefinition right after granting so the live instance is ready
            // for both AI (which never calls SetSelectedAbilityFromEntry) and the player.
            // Without this, AI units get a live instance with null AbilityDefinition, causing
            // BuildPreview to always return bIsValid=false and the AI to only ever move.
            InjectAbilityDefinition(Entry);
        }
    }

    // Append SkipTurn as the last ability for every unit — it is never defined
    // per-unit in UnitDefinition so designers don't need to add it manually.
    // Placed last so it always appears at the end of the action hotbar.
    if (SkipTurnDefinition && SkipTurnDefinition->AbilityClass)
    {
        FUnitAbilityEntry SkipTurnEntry;
        SkipTurnEntry.Definition          = SkipTurnDefinition;
        SkipTurnEntry.bGrantedAtBattleStart = true;
        SkipTurnEntry.bIsDefaultAction    = false;
        SkipTurnEntry.bIsMovementAbility  = false;

        GrantedBattleAbilities.Add(SkipTurnEntry);
        EnsureAbilityGranted(SkipTurnDefinition->AbilityClass);
        InjectAbilityDefinition(SkipTurnEntry);

        UE_LOG(LogTemp, Log,
            TEXT("[BattleModeComponent] SkipTurn appended as last ability for %s"),
            *GetNameSafe(GetOwner()));
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[BattleModeComponent] SkipTurnDefinition not set — universal SkipTurn ability not granted to %s"),
            *GetNameSafe(GetOwner()));
    }

    UE_LOG(LogTemp, Log, TEXT("[BattleModeComponent] Loaded %d battle abilities for %s"),
        GrantedBattleAbilities.Num(), *GetNameSafe(GetOwner()));
}

void UBattleModeComponent::EnsureAbilityGranted(TSubclassOf<UGameplayAbility> AbilityClass)
{
    if (!AbilityClass) { return; }

    ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
    if (!Character) { return; }

    UMyAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
    if (!ASC) { return; }

    if (ASC->FindAbilitySpecFromClass(AbilityClass)) { return; }

    if (Character->HasAuthority())
    {
        UE_LOG(LogTemp, Log, TEXT("[BattleModeComponent] Granting ability %s to %s"),
            *AbilityClass->GetName(), *GetNameSafe(GetOwner()));
        ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
    }
}

void UBattleModeComponent::HandleAbilitySelected()
{
    if (CurrentPreview)
    {
        CurrentPreview->ClearPreview();
        CurrentPreview = nullptr;
    }
}

void UBattleModeComponent::InjectAbilityDefinition(const FUnitAbilityEntry& Entry)
{
    if (!Entry.Definition || !Entry.Definition->AbilityClass)
    {
        return;
    }

    ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
    UMyAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
    if (!ASC)
    {
        return;
    }

    FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(Entry.Definition->AbilityClass);
    if (!Spec)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[BattleModeComponent] InjectAbilityDefinition: no spec found for %s on %s — was EnsureAbilityGranted called first?"),
            *Entry.Definition->AbilityClass->GetName(), *GetNameSafe(GetOwner()));
        return;
    }

    UBattleGameplayAbility* Instance = Cast<UBattleGameplayAbility>(Spec->GetPrimaryInstance());
    if (!Instance)
    {
        // GAS creates InstancedPerActor instances lazily on first activation, so the instance
        // may not exist yet immediately after GiveAbility. This is expected on first call;
        // the player path will re-inject via SetSelectedAbilityFromEntry before use,
        // and the AI path is guarded by ActionResolver falling back to CDO when null.
        UE_LOG(LogTemp, Verbose,
            TEXT("[BattleModeComponent] InjectAbilityDefinition: instance not yet available for %s on %s (will be set on first activation)"),
            *Entry.Definition->AbilityClass->GetName(), *GetNameSafe(GetOwner()));
        return;
    }

    Instance->AbilityDefinition = Entry.Definition;

    UE_LOG(LogTemp, Log,
        TEXT("[BattleModeComponent] Injected AbilityDefinition '%s' into '%s' on %s"),
        *Entry.Definition->DisplayName.ToString(),
        *Entry.Definition->AbilityClass->GetName(),
        *GetNameSafe(GetOwner()));
}

void UBattleModeComponent::SetSelectedAbilityFromEntry(const FUnitAbilityEntry& Entry)
{
    if (!Entry.Definition || !Entry.Definition->AbilityClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent] SetSelectedAbilityFromEntry: invalid entry"));
        return;
    }

    CurrentContext = {};
    CurrentContext.SourceActor = GetOwner();
    CurrentContext.AbilityClass = Entry.Definition->AbilityClass;

    // Re-inject to ensure the live instance is up to date.
    // Handles the case where the player switches between abilities that share the same
    // C++ class but have different AbilityDefinitions (e.g. two melee variants).
    InjectAbilityDefinition(Entry);

    const UBattleGameplayAbility* AbilityCDO =
        Entry.Definition->AbilityClass->GetDefaultObject<UBattleGameplayAbility>();

    if (AbilityCDO &&
        AbilityCDO->GetSelectionMode() == EBattleAbilitySelectionMode::TargetThenConfirmApproach)
    {
        CurrentContext.SelectionPhase = EBattleActionSelectionPhase::SelectingTarget;
    }
    else
    {
        CurrentContext.SelectionPhase = EBattleActionSelectionPhase::None;
    }

    UE_LOG(LogTemp, Log, TEXT("[BattleModeComponent] Selected ability: %s (%s)"),
        *Entry.Definition->DisplayName.ToString(),
        *Entry.Definition->AbilityClass->GetName());

    HandleAbilitySelected();
}

EBattleAbilitySelectionMode UBattleModeComponent::GetCurrentAbilitySelectionMode() const
{
    if (!CurrentContext.AbilityClass)
    {
        return EBattleAbilitySelectionMode::SingleConfirm;
    }

    const UBattleGameplayAbility* AbilityCDO =
        CurrentContext.AbilityClass->GetDefaultObject<UBattleGameplayAbility>();

    if (!AbilityCDO)
    {
        return EBattleAbilitySelectionMode::SingleConfirm;
    }

    return AbilityCDO->GetSelectionMode();
}

bool UBattleModeComponent::TryLockCurrentTarget()
{
    if (!CurrentContext.TargetActor) { return false; }

    CurrentContext.LockedTargetActor    = CurrentContext.TargetActor;
    CurrentContext.LockedTargetGridCoord = CurrentContext.HoveredGridCoord;
    CurrentContext.bHasLockedTarget      = true;
    CurrentContext.SelectionPhase        = EBattleActionSelectionPhase::SelectingApproachTile;
    return true;
}

void UBattleModeComponent::ClearLockedTarget()
{
    CurrentContext.LockedTargetActor         = nullptr;
    CurrentContext.LockedTargetGridCoord      = FIntPoint::ZeroValue;
    CurrentContext.SelectedApproachGridCoord  = FIntPoint::ZeroValue;
    CurrentContext.bHasLockedTarget           = false;
    CurrentContext.bHasSelectedApproachTile   = false;
    CurrentContext.SelectionPhase             = EBattleActionSelectionPhase::SelectingTarget;
}

bool UBattleModeComponent::IsCurrentAbilityInApproachSelection() const
{
    return CurrentContext.SelectionPhase == EBattleActionSelectionPhase::SelectingApproachTile;
}

bool UBattleModeComponent::SelectAbilityByIndex(int32 AbilityIndex)
{
    if (!GrantedBattleAbilities.IsValidIndex(AbilityIndex)) { return false; }

    SelectedAbilityIndex = AbilityIndex;
    SetSelectedAbilityFromEntry(GrantedBattleAbilities[AbilityIndex]);
    OnAbilitySelectionChanged.Broadcast(AbilityIndex);
    return true;
}

bool UBattleModeComponent::SelectAbilityByTag(const FGameplayTag& AbilityTag)
{
    for (int32 Index = 0; Index < GrantedBattleAbilities.Num(); ++Index)
    {
        const FUnitAbilityEntry& Entry = GrantedBattleAbilities[Index];
        if (Entry.Definition && Entry.Definition->AbilityTag == AbilityTag)
        {
            return SelectAbilityByIndex(Index);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent] SelectAbilityByTag: no ability found with tag %s"),
        *AbilityTag.ToString());
    return false;
}

bool UBattleModeComponent::SelectDefaultAbility()
{
    for (int32 Index = 0; Index < GrantedBattleAbilities.Num(); ++Index)
    {
        if (GrantedBattleAbilities[Index].bIsDefaultAction)
        {
            return SelectAbilityByIndex(Index);
        }
    }

    if (GrantedBattleAbilities.Num() > 0)
    {
        return SelectAbilityByIndex(0);
    }

    return false;
}

TSubclassOf<UBattleMovementAbility> UBattleModeComponent::GetEquippedMovementAbilityClass() const
{
    for (const FUnitAbilityEntry& Entry : GrantedBattleAbilities)
    {
        if (!Entry.bIsMovementAbility || !Entry.Definition || !Entry.Definition->AbilityClass)
        {
            continue;
        }

        if (Entry.Definition->AbilityClass->IsChildOf(UBattleMovementAbility::StaticClass()))
        {
            return TSubclassOf<UBattleMovementAbility>(Entry.Definition->AbilityClass);
        }
    }

    return nullptr;
}

bool UBattleModeComponent::PreviewCurrentAbilityExecution(FActionResult& ExecutionResult)
{
    if (!CurrentContext.AbilityClass) { return false; }

    ExecutionResult = ActionResolver->ResolveExecution(CurrentContext.AbilityClass, CurrentContext);

    if (!ExecutionResult.bIsValid)
    {
        UE_LOG(LogTemp, Log, TEXT("[BattleModeComponent] Execution invalid for %s"),
            *CurrentContext.AbilityClass->GetName());
        return false;
    }

    return true;
}

void UBattleModeComponent::RenderVisualPreview()
{
    if (!CurrentContext.AbilityClass) { return; }

    if (!CurrentPreview)
    {
        UBattleGameplayAbility* AbilityCDO =
            CurrentContext.AbilityClass->GetDefaultObject<UBattleGameplayAbility>();

        if (!AbilityCDO) { return; }

        TSubclassOf<UAbilityPreviewRenderer> PreviewClass = AbilityCDO->GetPreviewClass();
        if (!PreviewClass) { return; }

        CurrentPreview = NewObject<UAbilityPreviewRenderer>(this, PreviewClass);
        if (!CurrentPreview) { return; }

        CurrentPreview->Initialize(GetWorld());
    }

    CurrentPreview->UpdatePreview(CurrentContext);
}

void UBattleModeComponent::PreviewCurrentAbility(FActionResult& PreviewResult)
{
    if (!CurrentContext.AbilityClass) { return; }
 
    PreviewResult = ActionResolver->ResolvePreview(CurrentContext.AbilityClass, CurrentContext);
    CurrentContext.bIsActionValid = PreviewResult.bIsValid;
 
    if (PreviewResult.bIsValid)
    {
        CurrentContext.MovementTargetGridCoord    = PreviewResult.MovementTargetGridCoord;
        CurrentContext.MovementTargetWorldPosition = PreviewResult.MovementTargetWorldPosition;
    }
    else
    {
        CurrentContext.MovementTargetGridCoord    = FIntPoint::ZeroValue;
        CurrentContext.MovementTargetWorldPosition = FVector::ZeroVector;
    }
 
    RenderVisualPreview();
 
    // Resolve current AP from BattleState so the HUD always shows the live value.
    // APCost is 0 when the preview is invalid — the hotbar can grey out the display in that case.
    const int32 APCost = PreviewResult.bIsValid ? PreviewResult.ActionPointsCost : 0;
    int32 CurrentAP = 0;
 
    if (BattleState)
    {
        if (ACharacterBase* Character = Cast<ACharacterBase>(GetOwner()))
        {
            if (const FBattleUnitState* UnitState = BattleState->FindUnitState(Character))
            {
                CurrentAP = UnitState->CurrentActionPoints;
            }
        }
    }
 
    UE_LOG(LogTemp, Verbose,
        TEXT("[BattleModeComponent] PreviewUpdated — APCost: %d, RemainingAP: %d, Valid: %s"),
        APCost, CurrentAP, PreviewResult.bIsValid ? TEXT("true") : TEXT("false"));
 
    OnPreviewUpdated.Broadcast(APCost, CurrentAP, PreviewResult.bIsValid);
}

void UBattleModeComponent::Input_Confirm()
{
    if (!CurrentContext.AbilityClass) { return; }

    const EBattleAbilitySelectionMode SelectionMode = GetCurrentAbilitySelectionMode();

    if (SelectionMode == EBattleAbilitySelectionMode::SingleConfirm)
    {
        FActionResult ExecutionResult;
        if (!PreviewCurrentAbilityExecution(ExecutionResult)) { return; }

        CurrentContext.ActionPointsCost = ExecutionResult.ActionPointsCost;
        ExecuteCurrentAction(CurrentContext, ExecutionResult);
        return;
    }

    if (SelectionMode == EBattleAbilitySelectionMode::TargetThenConfirmApproach)
    {
        if (CurrentContext.SelectionPhase == EBattleActionSelectionPhase::SelectingTarget)
        {
            FActionResult PreviewResult;
            PreviewCurrentAbility(PreviewResult);

            if (!PreviewResult.bIsValid) { return; }
            if (!TryLockCurrentTarget()) { return; }

            PreviewCurrentAbility(PreviewResult);
            return;
        }

        if (CurrentContext.SelectionPhase == EBattleActionSelectionPhase::SelectingApproachTile)
        {
            FActionResult ExecutionResult;
            if (!PreviewCurrentAbilityExecution(ExecutionResult)) { return; }

            CurrentContext.ActionPointsCost = ExecutionResult.ActionPointsCost;
            ExecuteCurrentAction(CurrentContext, ExecutionResult);
        }
    }
}

void UBattleModeComponent::Input_Cancel()
{
    const EBattleAbilitySelectionMode SelectionMode = GetCurrentAbilitySelectionMode();

    if (SelectionMode == EBattleAbilitySelectionMode::TargetThenConfirmApproach &&
        CurrentContext.SelectionPhase == EBattleActionSelectionPhase::SelectingApproachTile)
    {
        ClearLockedTarget();

        FActionResult PreviewResult;
        PreviewCurrentAbility(PreviewResult);
        return;
    }

    IBattleInputReceiver::Input_Cancel();
}

void UBattleModeComponent::Input_Hover(const FVector& WorldLocation)
{
    UGridManager* GridManager = GetWorld()->GetSubsystem<UGridManager>();
    if (!GridManager) { return; }

    CurrentContext.SourceWorldPosition = GetOwner()->GetActorLocation();
    CurrentContext.HoveredWorldPosition = WorldLocation;

    CurrentContext.SourceGridCoord  = GridManager->WorldToGrid(CurrentContext.SourceWorldPosition);
    CurrentContext.HoveredGridCoord = GridManager->WorldToGrid(WorldLocation);

    const EBattleAbilitySelectionMode SelectionMode = GetCurrentAbilitySelectionMode();

    if (SelectionMode == EBattleAbilitySelectionMode::TargetThenConfirmApproach)
    {
        if (CurrentContext.SelectionPhase == EBattleActionSelectionPhase::SelectingTarget)
        {
            CurrentContext.TargetActor = GridManager->GetActorAt(CurrentContext.HoveredGridCoord);
        }
        else if (CurrentContext.SelectionPhase == EBattleActionSelectionPhase::SelectingApproachTile)
        {
            CurrentContext.TargetActor               = CurrentContext.LockedTargetActor;
            CurrentContext.SelectedApproachGridCoord  = CurrentContext.HoveredGridCoord;
            CurrentContext.bHasSelectedApproachTile   = true;
        }
    }
    else
    {
        CurrentContext.TargetActor = GridManager->GetActorAt(CurrentContext.HoveredGridCoord);
    }

    ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
    if (!Character) { return; }

    const FBattleUnitState* UnitState = BattleState
        ? BattleState->FindUnitState(Character)
        : nullptr;

    if (!UnitState)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[BattleModeComponent] Input_Hover: BattleUnitState not found for %s"),
            *GetNameSafe(Character));
        return;
    }

    CurrentContext.TraversalParams = UnitState->TraversalParams;
    CurrentContext.CachedPathCost  = GridManager->CalculatePathCost(
        CurrentContext.SourceGridCoord,
        CurrentContext.HoveredGridCoord,
        CurrentContext.TraversalParams
    );

    CurrentContext.bHasLineOfSight = HasLineOfSight();

    if (const FGridTile* Tile = GridManager->GetTile(CurrentContext.HoveredGridCoord))
    {
        CurrentContext.bIsTileBlocked = Tile->bBlocked;
    }
    else
    {
        CurrentContext.bIsTileBlocked = true;
    }

    FActionResult PreviewResult;
    PreviewCurrentAbility(PreviewResult);
}

void UBattleModeComponent::Input_SelectNextAction()
{
    const int32 AbilityCount = GrantedBattleAbilities.Num();
    SelectedAbilityIndex = (SelectedAbilityIndex + 1) % FMath::Max(1, AbilityCount);
    SelectAbilityByIndex(SelectedAbilityIndex);
}

void UBattleModeComponent::Input_SelectPreviousAction()
{
    const int32 AbilityCount = GrantedBattleAbilities.Num();
    SelectedAbilityIndex = (SelectedAbilityIndex - 1 + AbilityCount) % FMath::Max(1, AbilityCount);
    SelectAbilityByIndex(SelectedAbilityIndex);
}

bool UBattleModeComponent::ExecuteActionFromAI(const FActionContext& InContext)
{
    CurrentContext = InContext;

    // Late-inject AbilityDefinition into the live ASC instance before resolving execution.
    // The AI's GatherAbilityCandidates populates Context.AbilityDefinition from Entry.Definition,
    // but the live instance may still have null AbilityDefinition if GAS created it lazily
    // after InitializeAbilitiesFromDefinition ran during EnterMode.
    if (InContext.AbilityDefinition && InContext.AbilityClass)
    {
        ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
        UMyAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
        if (ASC)
        {
            if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(InContext.AbilityClass))
            {
                if (UBattleGameplayAbility* Instance =
                    Cast<UBattleGameplayAbility>(Spec->GetPrimaryInstance()))
                {
                    if (!Instance->AbilityDefinition)
                    {
                        UE_LOG(LogTemp, Log,
                            TEXT("[BattleModeComponent] ExecuteActionFromAI: late-injecting AbilityDefinition '%s' into '%s' on %s"),
                            *InContext.AbilityDefinition->DisplayName.ToString(),
                            *InContext.AbilityClass->GetName(),
                            *GetNameSafe(GetOwner()));
                        Instance->AbilityDefinition = InContext.AbilityDefinition;
                    }
                }
            }
        }
    }

    FActionResult ExecutionResult;
    if (!PreviewCurrentAbilityExecution(ExecutionResult)) { return false; }

    CurrentContext.ActionPointsCost = ExecutionResult.ActionPointsCost;
    return ExecuteCurrentAction(CurrentContext, ExecutionResult);
}

bool UBattleModeComponent::ExecuteCurrentAction(const FActionContext& Context, const FActionResult& Result)
{
    if (!Context.AbilityClass) { return false; }

    ACharacterBase* Character = Cast<ACharacterBase>(Context.SourceActor);
    if (!Character) { return false; }

    UMyAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
    if (!ASC) { return false; }

    UE_LOG(LogTemp, Log, TEXT("[BattleModeComponent] Activating ability %s for %s"),
        *Context.AbilityClass->GetName(), *GetNameSafe(Character));

    HandleActionStarted();

    const bool bActivated = ASC->TryActivateAbilityByClass(Context.AbilityClass);

    if (!bActivated)
    {
        // Activation failed (e.g. blocked by stun tag).
        // HandleActionStarted already locked input and the hotbar,
        // so we must call HandleActionFinished to restore them or combat freezes.
        UE_LOG(LogTemp, Error,
            TEXT("[BattleModeComponent] Failed to activate %s for %s -- restoring input"),
            *GetNameSafe(Context.AbilityClass), *GetNameSafe(Character));
        HandleActionFinished();
    }

    return bActivated;
}

void UBattleModeComponent::HandleActionStarted()
{
    if (bActionInProgress) { return; }

    bActionInProgress = true;

    ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
    UE_LOG(LogTemp, Log, TEXT("[BattleModeComponent] Broadcasting ActionStarted for %s"),
        *GetNameSafe(Character));
    OnActionUseStarted.Broadcast(Character);
}

void UBattleModeComponent::HandleActionFinished()
{
    if (!bActionInProgress)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent] Ignoring duplicate ActionFinished."));
        return;
    }

    bActionInProgress = false;
    ClearLockedTarget();

    ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
    UE_LOG(LogTemp, Log, TEXT("[BattleModeComponent] Broadcasting ActionFinished for %s"),
        *GetNameSafe(Character));
    OnActionUseFinished.Broadcast(Character);

    if (BattleState && Character)
    {
        const FBattleUnitState* UnitState = BattleState->FindUnitState(Character);
        if (UnitState && UnitState->CurrentActionPoints == 0)
        {
            OnActionPointsDepleted.Broadcast(Character);
        }
    }
}

void UBattleModeComponent::UpdateOccupancy(const FIntPoint& NewGridCoord) const
{
    UGridManager* GridManager = GetWorld()->GetSubsystem<UGridManager>();
    if (!GridManager) { return; }

    GridManager->UpdateActorPosition(GetOwner(), NewGridCoord);
}

bool UBattleModeComponent::HasLineOfSight() const
{
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit,
        CurrentContext.SourceWorldPosition,
        CurrentContext.HoveredWorldPosition,
        ECC_Visibility,
        Params
    );

    return !bHit;
}
// ---------------------------------------------------------------------------
// Per-definition cooldown map
// ---------------------------------------------------------------------------

bool UBattleModeComponent::IsDefinitionOnCooldown(const UAbilityDefinition* Def) const
{
    if (!Def) { return false; }
    const int32* Turns = DefinitionCooldowns.Find(Def);
    return Turns && *Turns > 0;
}

int32 UBattleModeComponent::GetDefinitionCooldownTurns(const UAbilityDefinition* Def) const
{
    if (!Def) { return 0; }
    const int32* Turns = DefinitionCooldowns.Find(Def);
    return Turns ? *Turns : 0;
}

void UBattleModeComponent::StartDefinitionCooldown(const UAbilityDefinition* Def, int32 Turns)
{
    if (!Def || Turns <= 0) { return; }
    DefinitionCooldowns.Add(Def, Turns);
    UE_LOG(LogTemp, Log,
        TEXT("[BattleModeComponent] Cooldown started: '%s' = %d turns on %s"),
        *Def->DisplayName.ToString(), Turns, *GetNameSafe(GetOwner()));
}

void UBattleModeComponent::TickDefinitionCooldowns()
{
    // Called once per player turn start from CombatManager.
    // Decrements each active cooldown and removes entries that expire.
    TArray<TObjectPtr<const UAbilityDefinition>> Expired;

    for (auto& Pair : DefinitionCooldowns)
    {
        Pair.Value--;
        UE_LOG(LogTemp, Log,
            TEXT("[BattleModeComponent] Cooldown tick: '%s' = %d turns remaining on %s"),
            *GetNameSafe(Pair.Key), Pair.Value, *GetNameSafe(GetOwner()));

        if (Pair.Value <= 0)
        {
            Expired.Add(Pair.Key);
        }
    }

    for (const auto& Def : Expired)
    {
        DefinitionCooldowns.Remove(Def);
        UE_LOG(LogTemp, Log,
            TEXT("[BattleModeComponent] Cooldown expired: '%s' on %s"),
            *GetNameSafe(Def), *GetNameSafe(GetOwner()));
    }
}

TArray<FSavedCooldown> UBattleModeComponent::CollectCooldownSaveData() const
{
    TArray<FSavedCooldown> Result;
    Result.Reserve(DefinitionCooldowns.Num());

    for (const auto& Pair : DefinitionCooldowns)
    {
        if (!Pair.Key || Pair.Value <= 0) { continue; }

        FSavedCooldown Entry;
        Entry.DefinitionPath  = FSoftObjectPath(Pair.Key->GetPathName());
        Entry.TurnsRemaining  = Pair.Value;
        Result.Add(MoveTemp(Entry));
    }

    UE_LOG(LogTemp, Log,
        TEXT("[BattleModeComponent] CollectCooldownSaveData: %d cooldown(s) collected for %s"),
        Result.Num(), *GetNameSafe(GetOwner()));

    return Result;
}

void UBattleModeComponent::RestoreCooldowns(const TArray<FSavedCooldown>& SavedCooldowns)
{
    DefinitionCooldowns.Reset();

    for (const FSavedCooldown& Saved : SavedCooldowns)
    {
        if (Saved.TurnsRemaining <= 0) { continue; }

        UAbilityDefinition* Def = Cast<UAbilityDefinition>(Saved.DefinitionPath.TryLoad());
        if (!Def)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[BattleModeComponent] RestoreCooldowns: could not load AbilityDefinition at '%s' — skipped"),
                *Saved.DefinitionPath.ToString());
            continue;
        }

        DefinitionCooldowns.Add(Def, Saved.TurnsRemaining);

        UE_LOG(LogTemp, Log,
            TEXT("[BattleModeComponent] RestoreCooldowns: '%s' restored with %d turns on %s"),
            *GetNameSafe(Def), Saved.TurnsRemaining, *GetNameSafe(GetOwner()));
    }
}