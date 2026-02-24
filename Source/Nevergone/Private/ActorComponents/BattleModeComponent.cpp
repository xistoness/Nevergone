// Copyright Xyzto Works


#include "ActorComponents/BattleModeComponent.h"

#include "ActorComponents/MyAbilitySystemComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/GA_Move_Simple.h"
#include "Characters/Abilities/AbilityPreview/AbilityPreviewRenderer.h"
#include "GameMode/Combat/Resolvers/ActionResolver.h"
#include "Level/GridManager.h"
#include "WorldPartition/HLOD/HLODActor.h"

UBattleModeComponent::UBattleModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBattleModeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UBattleModeComponent::EnterMode()
{
	Super::EnterMode();
	SetComponentTickEnabled(true);
	
	UE_LOG(LogTemp, Warning, TEXT("Owner class: %s"),
	*GetOwner()->GetClass()->GetName());
	
	UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Entering battle mode..."));
	ActionResolver = NewObject<UActionResolver>(this);

	// Default action is MOVE
	SetDefaultMoveAction();
	
	ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
	if (!Character)
		return;

	UMyAbilitySystemComponent* ASC =
		Character->GetAbilitySystemComponent();

	if (!ASC)
		return;

	ASC->InitAbilityActorInfo(Character, Character);
	// Only grant if not already granted
	if (!ASC->FindAbilitySpecFromClass(DefaultMoveAbilityClass))
	{
		if (Character->HasAuthority())
		{
			UE_LOG(LogTemp, Warning, TEXT("Ability class valid? %s"), DefaultMoveAbilityClass ? TEXT("YES") : TEXT("NO"));
			ASC->GiveAbility(FGameplayAbilitySpec(DefaultMoveAbilityClass, 1, INDEX_NONE, this));	
		}
	}
	FGameplayAbilitySpec* Spec =
	ASC->FindAbilitySpecFromClass(DefaultMoveAbilityClass);

	UE_LOG(LogTemp, Warning, TEXT("After GiveAbility - Spec exists? %s"),
		Spec ? TEXT("YES") : TEXT("NO"));
}

void UBattleModeComponent::ExitMode()
{
	Super::ExitMode();
	SetComponentTickEnabled(false);
}

void UBattleModeComponent::HandleAbilitySelected()
{
	if (CurrentPreview)
	{
		CurrentPreview->ClearPreview();
		CurrentPreview = nullptr;
	}
	
}

bool UBattleModeComponent::PreviewCurrentAbilityExecution(FActionResult& ExecutionResult)
{
	if (!CurrentContext.AbilityClass)
		return false;
	
	UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Trying to execute: %s"), *CurrentContext.AbilityClass.Get()->GetName());
	ExecutionResult = ActionResolver->ResolveExecution(
		CurrentContext.AbilityClass,
		CurrentContext);

	if (!ExecutionResult.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Execution invalid"));
		return false;
	}
	return true;
}

void UBattleModeComponent::RenderVisualPreview()
{

	if (!CurrentPreview)
	{
		ACharacterBase* Character =
			Cast<ACharacterBase>(GetOwner());

		if (!Character)
			return;

		UMyAbilitySystemComponent* ASC =
			Character->GetAbilitySystemComponent();

		if (!ASC)
			return;
	
		UBattleGameplayAbility* AbilityCDO =
			CurrentContext.AbilityClass->GetDefaultObject<UBattleGameplayAbility>();

		if (!AbilityCDO)
			return;
			
		CurrentPreview = NewObject<UAbilityPreviewRenderer>(this, AbilityCDO->GetPreviewClass());
		CurrentPreview->Initialize(GetWorld());
	}
		
	CurrentPreview->UpdatePreview(CurrentContext);
}

void UBattleModeComponent::PreviewCurrentAbility(FActionResult& PreviewResult)
{
	if (!CurrentContext.AbilityClass)
		return;
	
	UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Trying to execute: %s"), *CurrentContext.AbilityClass.Get()->GetName());
	PreviewResult = ActionResolver->ResolvePreview(
		CurrentContext.AbilityClass,
		CurrentContext);
	
	CurrentContext.bIsActionValid = PreviewResult.bIsValid;
	RenderVisualPreview();
}

void UBattleModeComponent::Input_Confirm()
{
	FActionResult ExecutionResult;
	if (!PreviewCurrentAbilityExecution(ExecutionResult)) return;

	CurrentContext.ActionPointsCost = ExecutionResult.ActionPointsCost;
	ExecuteCurrentAction(CurrentContext, ExecutionResult);
}

void UBattleModeComponent::Input_Cancel()
{
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
	CurrentContext.TargetWorldPosition = WorldLocation;
	
	CurrentContext.SourceGridCoord = GridManager->WorldToGrid(CurrentContext.SourceWorldPosition);
	CurrentContext.TargetGridCoord = GridManager->WorldToGrid(WorldLocation);
	
	CurrentContext.TargetActor = GridManager->GetActorAt(CurrentContext.TargetGridCoord);
	
	CurrentContext.CachedPathCost = GridManager->CalculatePathCost(CurrentContext.SourceGridCoord, CurrentContext.TargetGridCoord);
	CurrentContext.bHasLineOfSight = HasLineOfSight();
	if (const FGridTile* Tile = GridManager->GetTile(CurrentContext.TargetGridCoord))
	{
		CurrentContext.bIsTileBlocked = Tile->bBlocked;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Tile is invalid..."));
	}
	
	FActionResult PreviewResult;
	PreviewCurrentAbility(PreviewResult);
}

void UBattleModeComponent::SetDefaultMoveAction()
{
	CurrentContext = {};

	CurrentContext.SourceActor = GetOwner();
	CurrentContext.Ability = DefaultMoveAbilityData;
	CurrentContext.AbilityClass = DefaultMoveAbilityClass;
}

void UBattleModeComponent::ExecuteCurrentAction(const FActionContext& Context, const FActionResult& Result)
{
	UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Trying to execute ability..."));
	if (!Context.AbilityClass)
		return;

	ACharacterBase* Character =
		Cast<ACharacterBase>(Context.SourceActor);

	if (!Character)
		return;

	UMyAbilitySystemComponent* ASC =
		Character->GetAbilitySystemComponent();

	if (!ASC)
		return;
	
	UBattleGameplayAbility* AbilityCDO =
		Context.AbilityClass->GetDefaultObject<UBattleGameplayAbility>();

	if (!AbilityCDO)
		return;
	
	UE_LOG(LogTemp, Warning, TEXT("[BattleModeComponent]: Calling ASC to activate ability!"));
	
	// Try to activate ability and checks if it worked
	ASC->TryActivateAbilityByClass(Context.AbilityClass);
}

void UBattleModeComponent::UpdateOccupancy() const
{
	UGridManager* GridManager = GetWorld()->GetSubsystem<UGridManager>();
	if (!GridManager)
	{
		return;
	}
	
	GridManager->UpdateActorPosition(GetOwner(), CurrentContext.TargetGridCoord);
}

void UBattleModeComponent::ConsumeActionPoints() const
{
	if (ACharacterBase* Character = Cast<ACharacterBase>(GetOwner()))
	{
		UUnitStatsComponent* CharacterUnitStats = Character->GetUnitStats();
		CharacterUnitStats->SetCurrentActionPoints(CharacterUnitStats->GetCurrentActionPoints() - CurrentContext.ActionPointsCost);
		if (CharacterUnitStats->GetCurrentActionPoints() == 0)
		{
			OnActionPointsDepleted.Broadcast(Character);
		}
	}
}

bool UBattleModeComponent::HasLineOfSight() const
{
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());

	bool bHit =
		GetWorld()->LineTraceSingleByChannel(
			Hit,
			CurrentContext.SourceWorldPosition,
			CurrentContext.TargetWorldPosition,
			ECC_Visibility,
			Params
		);
	if (!bHit)
		return true;
	
	return false;
}
