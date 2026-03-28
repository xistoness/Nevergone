// Copyright Xyzto Works

#include "ActorComponents/BattleModeComponent.h"

#include "Abilities/GameplayAbility.h"
#include "ActorComponents/MyAbilitySystemComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/BattleGameplayAbility.h"
#include "Characters/Abilities/BattleMovementAbility.h"
#include "Characters/Abilities/AbilityPreview/AbilityPreviewRenderer.h"
#include "Data/UnitDefinition.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "GameMode/Combat/Resolvers/ActionResolver.h"
#include "Level/GridManager.h"

UBattleModeComponent::UBattleModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBattleModeComponent::SetCombatEventBus(UCombatEventBus* InBus)
{
	CombatEventBus = InBus;
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

	UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Entering battle mode..."));

	ActionResolver = NewObject<UActionResolver>(this);

	ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
	if (!Character)
	{
		return;
	}

	UMyAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}
	
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
	if (!Stats) return;
	
	const UUnitDefinition* Definition = Stats->GetDefinition();

	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: UnitDefinition is null"));
		return;
	}

	for (const FUnitAbilityEntry& Entry : Definition->BattleAbilities)
	{
		if (!Entry.AbilityClass)
		{
			continue;
		}

		GrantedBattleAbilities.Add(Entry);

		if (Entry.bGrantedAtBattleStart)
		{
			EnsureAbilityGranted(Entry.AbilityClass);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Loaded %d battle abilities from definition"),
		GrantedBattleAbilities.Num());
}

void UBattleModeComponent::EnsureAbilityGranted(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilityClass)
	{
		return;
	}

	ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
	if (!Character)
	{
		return;
	}

	UMyAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	if (ASC->FindAbilitySpecFromClass(AbilityClass))
	{
		return;
	}

	if (Character->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Granting ability %s"), *AbilityClass->GetName());
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

void UBattleModeComponent::SetSelectedAbilityFromEntry(const FUnitAbilityEntry& Entry)
{
	CurrentContext = {};
	CurrentContext.SourceActor = GetOwner();
	CurrentContext.AbilityClass = Entry.AbilityClass;
	CurrentContext.Ability = Entry.AbilityData;

	const UBattleGameplayAbility* AbilityCDO =
		Entry.AbilityClass
			? Entry.AbilityClass->GetDefaultObject<UBattleGameplayAbility>()
			: nullptr;

	if (AbilityCDO &&
		AbilityCDO->GetSelectionMode() == EBattleAbilitySelectionMode::TargetThenConfirmApproach)
	{
		CurrentContext.SelectionPhase = EBattleActionSelectionPhase::SelectingTarget;
	}
	else
	{
		CurrentContext.SelectionPhase = EBattleActionSelectionPhase::None;
	}

	UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Selected ability: %s"), *Entry.AbilityClass->GetName());

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
	if (!CurrentContext.TargetActor)
	{
		return false;
	}

	CurrentContext.LockedTargetActor = CurrentContext.TargetActor;
	CurrentContext.LockedTargetGridCoord = CurrentContext.HoveredGridCoord;
	CurrentContext.bHasLockedTarget = true;
	CurrentContext.SelectionPhase = EBattleActionSelectionPhase::SelectingApproachTile;
	return true;
}

void UBattleModeComponent::ClearLockedTarget()
{
	CurrentContext.LockedTargetActor = nullptr;
	CurrentContext.LockedTargetGridCoord = FIntPoint::ZeroValue;
	CurrentContext.SelectedApproachGridCoord = FIntPoint::ZeroValue;
	CurrentContext.bHasLockedTarget = false;
	CurrentContext.bHasSelectedApproachTile = false;
	CurrentContext.SelectionPhase = EBattleActionSelectionPhase::SelectingTarget;
}

bool UBattleModeComponent::IsCurrentAbilityInApproachSelection() const
{
	return CurrentContext.SelectionPhase == EBattleActionSelectionPhase::SelectingApproachTile;
}

bool UBattleModeComponent::SelectAbilityByIndex(int32 AbilityIndex)
{
	if (!GrantedBattleAbilities.IsValidIndex(AbilityIndex))
	{
		return false;
	}

	SelectedAbilityIndex = AbilityIndex;
	SetSelectedAbilityFromEntry(GrantedBattleAbilities[AbilityIndex]);
	return true;
}

bool UBattleModeComponent::SelectAbilityByActionId(FName ActionId)
{
	for (int32 Index = 0; Index < GrantedBattleAbilities.Num(); ++Index)
	{
		if (GrantedBattleAbilities[Index].ActionId == ActionId)
		{
			return SelectAbilityByIndex(Index);
		}
	}

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
		if (!Entry.bIsMovementAbility || !Entry.AbilityClass)
		{
			continue;
		}

		if (Entry.AbilityClass->IsChildOf(UBattleMovementAbility::StaticClass()))
		{
			return TSubclassOf<UBattleMovementAbility>(Entry.AbilityClass);
		}
	}

	return nullptr;
}

bool UBattleModeComponent::PreviewCurrentAbilityExecution(FActionResult& ExecutionResult)
{
	if (!CurrentContext.AbilityClass)
	{
		return false;
	}

	//UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Trying to build execution: %s"), *CurrentContext.AbilityClass->GetName());

	ExecutionResult = ActionResolver->ResolveExecution(CurrentContext.AbilityClass, CurrentContext);

	if (!ExecutionResult.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Execution invalid"));
		return false;
	}

	return true;
}

void UBattleModeComponent::RenderVisualPreview()
{
	if (!CurrentContext.AbilityClass)
	{
		return;
	}

	if (!CurrentPreview)
	{
		UBattleGameplayAbility* AbilityCDO =
			CurrentContext.AbilityClass->GetDefaultObject<UBattleGameplayAbility>();

		if (!AbilityCDO)
		{
			return;
		}

		TSubclassOf<UAbilityPreviewRenderer> PreviewClass = AbilityCDO->GetPreviewClass();
		if (!PreviewClass)
		{
			return;
		}

		CurrentPreview = NewObject<UAbilityPreviewRenderer>(this, PreviewClass);
		if (!CurrentPreview)
		{
			return;
		}

		CurrentPreview->Initialize(GetWorld());
	}

	CurrentPreview->UpdatePreview(CurrentContext);
}

void UBattleModeComponent::PreviewCurrentAbility(FActionResult& PreviewResult)
{
	if (!CurrentContext.AbilityClass)
	{
		return;
	}

	PreviewResult = ActionResolver->ResolvePreview(CurrentContext.AbilityClass, CurrentContext);
	CurrentContext.bIsActionValid = PreviewResult.bIsValid;

	if (PreviewResult.bIsValid)
	{
		CurrentContext.MovementTargetGridCoord = PreviewResult.MovementTargetGridCoord;
		CurrentContext.MovementTargetWorldPosition = PreviewResult.MovementTargetWorldPosition;
	}
	else
	{
		CurrentContext.MovementTargetGridCoord = FIntPoint::ZeroValue;
		CurrentContext.MovementTargetWorldPosition = FVector::ZeroVector;
	}

	RenderVisualPreview();
}

void UBattleModeComponent::Input_Confirm()
{
	if (!CurrentContext.AbilityClass)
	{
		return;
	}

	const EBattleAbilitySelectionMode SelectionMode = GetCurrentAbilitySelectionMode();

	// Standard abilities: execute immediately on confirm
	if (SelectionMode == EBattleAbilitySelectionMode::SingleConfirm)
	{
		FActionResult ExecutionResult;
		if (!PreviewCurrentAbilityExecution(ExecutionResult))
		{
			return;
		}

		CurrentContext.ActionPointsCost = ExecutionResult.ActionPointsCost;
		ExecuteCurrentAction(CurrentContext, ExecutionResult);
		return;
	}

	// Two-step abilities: first lock target, then confirm approach tile
	if (SelectionMode == EBattleAbilitySelectionMode::TargetThenConfirmApproach)
	{
		if (CurrentContext.SelectionPhase == EBattleActionSelectionPhase::SelectingTarget)
		{
			FActionResult PreviewResult;
			PreviewCurrentAbility(PreviewResult);

			if (!PreviewResult.bIsValid)
			{
				return;
			}

			if (!TryLockCurrentTarget())
			{
				return;
			}

			PreviewCurrentAbility(PreviewResult);
			return;
		}

		if (CurrentContext.SelectionPhase == EBattleActionSelectionPhase::SelectingApproachTile)
		{
			FActionResult ExecutionResult;
			if (!PreviewCurrentAbilityExecution(ExecutionResult))
			{
				return;
			}

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
	if (!GridManager)
	{
		return;
	}

	CurrentContext.SourceWorldPosition = GetOwner()->GetActorLocation();
	CurrentContext.HoveredWorldPosition = WorldLocation;

	CurrentContext.SourceGridCoord = GridManager->WorldToGrid(CurrentContext.SourceWorldPosition);
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
			CurrentContext.TargetActor = CurrentContext.LockedTargetActor;
			CurrentContext.SelectedApproachGridCoord = CurrentContext.HoveredGridCoord;
			CurrentContext.bHasSelectedApproachTile = true;
		}
	}
	else
	{
		// Standard abilities keep using hovered tile / hovered actor directly
		CurrentContext.TargetActor = GridManager->GetActorAt(CurrentContext.HoveredGridCoord);
	}

	ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
	if (!Character || !Character->GetUnitStats())
	{
		return;
	}

	CurrentContext.TraversalParams = Character->GetUnitStats()->GetTraversalParams();
	CurrentContext.CachedPathCost = GridManager->CalculatePathCost(
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
		UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Hovered tile is invalid"));
	}

	FActionResult PreviewResult;
	PreviewCurrentAbility(PreviewResult);
}

void UBattleModeComponent::Input_SelectNextAction()
{
	int32 AbilityCount = GrantedBattleAbilities.Num();
	SelectedAbilityIndex += 1;
	if (SelectedAbilityIndex >= AbilityCount)
	{
		SelectedAbilityIndex = 0;
	}
	
	SelectAbilityByIndex(SelectedAbilityIndex);
}

void UBattleModeComponent::Input_SelectPreviousAction()
{
	int32 AbilityCount = GrantedBattleAbilities.Num();
	SelectedAbilityIndex -= 1;

	if (SelectedAbilityIndex < 0)
	{
		SelectedAbilityIndex = AbilityCount - 1;
	}
	
	SelectAbilityByIndex(SelectedAbilityIndex);	
}


bool UBattleModeComponent::ExecuteActionFromAI(const FActionContext& InContext)
{
	CurrentContext = InContext;

	FActionResult ExecutionResult;
	if (!PreviewCurrentAbilityExecution(ExecutionResult))
	{
		return false;
	}

	CurrentContext.ActionPointsCost = ExecutionResult.ActionPointsCost;

	// ExecuteCurrentAction can fail if TryActivateAbilityByClass rejects the
	// ability (e.g. tag requirements not met). Propagate the result so the
	// AI executor knows whether to wait for OnActionUseFinished or bail out.
	return ExecuteCurrentAction(CurrentContext, ExecutionResult);
}

bool UBattleModeComponent::ExecuteCurrentAction(const FActionContext& Context, const FActionResult& Result)
{
	UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Trying to execute ability..."));

	if (!Context.AbilityClass) return false;

	ACharacterBase* Character = Cast<ACharacterBase>(Context.SourceActor);
	if (!Character) return false;

	UMyAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!ASC) return false;

	UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Calling ASC to activate ability!"));

	// Start must be flagged before activation — synchronous abilities call
	// HandleActionFinished during TryActivateAbilityByClass, before this
	// function returns, so the order here is critical
	HandleActionStarted();

	const bool bActivated = ASC->TryActivateAbilityByClass(Context.AbilityClass);

	if (!bActivated)
	{
		// Activation failed — roll back the started state
		bActionInProgress = false;
		UE_LOG(LogTemp, Error, TEXT("[BattleModeComponent]: Failed to activate ability %s for %s"),
			*GetNameSafe(Context.AbilityClass),
			*GetNameSafe(Character));
	}

	return bActivated;
}

void UBattleModeComponent::HandleActionStarted()
{
	if (bActionInProgress)
	{
		return;
	}

	bActionInProgress = true;
	
	ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());

	UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Broadcasting ActionStarted!"));
	OnActionUseStarted.Broadcast(Character);
}

void UBattleModeComponent::HandleActionFinished()
{
	if (!bActionInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Ignoring duplicate ActionFinished."));
		return;
	}

	bActionInProgress = false;

	// Reset selection state so the next confirm starts a fresh target
	// selection instead of falling into the approach-tile branch with stale data
	ClearLockedTarget();

	ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());

	UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Broadcasting ActionFinished!"));
	OnActionUseFinished.Broadcast(Character);

	if (UUnitStatsComponent* UnitStats = Character->GetUnitStats())
	{
		if (UnitStats->GetCurrentActionPoints() == 0)
		{
			OnActionPointsDepleted.Broadcast(Character);
		}
	}
}

void UBattleModeComponent::UpdateOccupancy(const FIntPoint& NewGridCoord) const
{
	UGridManager* GridManager = GetWorld()->GetSubsystem<UGridManager>();
	if (!GridManager)
	{
		return;
	}

	GridManager->UpdateActorPosition(GetOwner(), NewGridCoord);
}

void UBattleModeComponent::ConsumeActionPoints(int32 ExplicitCost) const
{
	if (UUnitStatsComponent* Stats = GetUnitStats())
	{
		Stats->SetCurrentActionPoints(Stats->GetCurrentActionPoints() - ExplicitCost);
	}
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