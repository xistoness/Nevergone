// Copyright Xyzto Works

#include "Characters/Abilities/GA_Move_Simple.h"

#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/AbilityPreview/Preview_Move.h"
#include "Characters/Abilities/TargetingRules/MoveRangeRule.h"
#include "Characters/Abilities/TargetingRules/TileNotBlockedRule.h"
#include "Characters/Abilities/TargetingRules/TileNotOccupiedRule.h"
#include "Level/GridManager.h"
#include "Nevergone.h"
#include "Types/BattleTypes.h"

UGA_Move_Simple::UGA_Move_Simple()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Ability_Move);
	SetAssetTags(Tags);

	TargetPolicy.AddRule(MakeUnique<MoveRangeRule>());
	TargetPolicy.AddRule(MakeUnique<TileNotBlockedRule>());
	TargetPolicy.AddRule(MakeUnique<TileNotOccupiedRule>());

	PreviewRendererClass = UPreview_Move::StaticClass();
}

FActionResult UGA_Move_Simple::BuildMovementPreview(const FActionContext& Context) const
{
	//UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: Trying to build movement preview..."));

	FActionResult Result;

	ACharacterBase* Character = Cast<ACharacterBase>(Context.SourceActor);
	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: Character is invalid..."));
		return Result;
	}

	if (!TargetPolicy.IsValid(Context))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: Target policy is invalid..."));
		return Result;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: World is invalid..."));
		return Result;
	}

	UGridManager* Grid = World->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: Grid is invalid..."));
		return Result;
	}

	const FIntPoint TargetCoord = Grid->WorldToGrid(Context.HoveredWorldPosition);
	const FGridTile* Tile = Grid->GetTile(TargetCoord);
	if (!Tile)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: Target tile is invalid..."));
		return Result;
	}

	UUnitStatsComponent* UnitStats = Character->GetUnitStats();
	if (!UnitStats)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: UnitStats is invalid..."));
		return Result;
	}

	const int32 Distance = Context.CachedPathCost;
	const int32 Speed = FMath::Max(1, UnitStats->GetSpeed());
	const FGridTraversalParams TraversalParams = UnitStats->GetTraversalParams();

	if (Distance == INDEX_NONE || Distance < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: CachedPathCost is invalid..."));
		return Result;
	}

	TArray<FIntPoint> PathCoords;
	if (!Grid->FindPath(Context.SourceGridCoord, TargetCoord, TraversalParams, PathCoords))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: Pathfinding failed..."));
		return Result;
	}

	Result.PathPoints.Reset();
	Result.PathPoints.Reserve(PathCoords.Num());

	for (const FIntPoint& Coord : PathCoords)
	{
		const FVector GroundPoint = Grid->GridToWorld(Coord);
		Result.PathPoints.Add(Character->GetGroundAlignedLocation(GroundPoint));
	}

	Result.bIsValid = true;
	Result.MovementTargetGridCoord = TargetCoord;
	Result.MovementTargetWorldPosition = Character->GetGroundAlignedLocation(Tile->WorldLocation);
	Result.ActionPointsCost = (Distance + Speed - 1) / Speed;

	return Result;
}

FActionResult UGA_Move_Simple::BuildMovementExecution(const FActionContext& Context) const
{
	return BuildMovementPreview(Context);
}

bool UGA_Move_Simple::ExecuteMovementPlan(
	ACharacterBase* Character,
	const FActionResult& MovementResult,
	const FSimpleDelegate& OnMovementFinished
)
{
	if (!Character || !MovementResult.bIsValid)
	{
		return false;
	}

	if (MovementFinishedDelegateHandle.IsValid())
	{
		Character->OnPathMoveFinished.Remove(MovementFinishedDelegateHandle);
		MovementFinishedDelegateHandle.Reset();
	}

	PendingMovementFinishedDelegate.Unbind();

	if (OnMovementFinished.IsBound())
	{
		PendingMovementFinishedDelegate = OnMovementFinished;

		MovementFinishedDelegateHandle =
			Character->OnPathMoveFinished.AddUObject(this, &UGA_Move_Simple::HandleExternalMovementFinished);
	}

	Character->MoveToLocation(MovementResult.MovementTargetWorldPosition, MovementResult.PathPoints);
	return true;
}

void UGA_Move_Simple::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: Ability activated..."));

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	ACharacterBase* Character = Cast<ACharacterBase>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UBattleModeComponent* BattleMode = Character->GetBattleModeComponent();
	if (!BattleMode)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const FActionContext& Context = BattleMode->GetCurrentContext();
	const FActionResult ExecutionResult = BuildMovementExecution(Context);

	if (!ExecutionResult.bIsValid)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	CachedCharacter = Character;
	CachedActionPointsCost = ExecutionResult.ActionPointsCost;
	CachedDestinationGridCoord = ExecutionResult.MovementTargetGridCoord;

	Character->OnPathMoveFinished.RemoveAll(this);
	Character->OnPathMoveFinished.AddUObject(this, &UGA_Move_Simple::FinalizeAbilityExecution);

	if (!ExecuteMovementPlan(Character, ExecutionResult, FSimpleDelegate()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UGA_Move_Simple::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	if (CachedCharacter)
	{
		CachedCharacter->OnPathMoveFinished.RemoveAll(this);

		if (MovementFinishedDelegateHandle.IsValid())
		{
			CachedCharacter->OnPathMoveFinished.Remove(MovementFinishedDelegateHandle);
			MovementFinishedDelegateHandle.Reset();
		}
	}

	PendingMovementFinishedDelegate.Unbind();
	CachedCharacter = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Move_Simple::ApplyAbilityCompletionEffects()
{
	Super::ApplyAbilityCompletionEffects();
	UE_LOG(LogTemp, Warning, TEXT("[GA_Move_Simple]: Ability applying completion effects"));
	if (!CachedCharacter)
	{
		return;
	}

	
	if (UBattleModeComponent* BattleMode = CachedCharacter->GetBattleModeComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Move_Simple]: Ability calling BattleMode functions"));
		BattleMode->UpdateOccupancy(CachedDestinationGridCoord);
		BattleMode->ConsumeActionPoints(CachedActionPointsCost);
	}
}

void UGA_Move_Simple::HandleExternalMovementFinished()
{
	if (CachedCharacter && MovementFinishedDelegateHandle.IsValid())
	{
		CachedCharacter->OnPathMoveFinished.Remove(MovementFinishedDelegateHandle);
		MovementFinishedDelegateHandle.Reset();
	}

	PendingMovementFinishedDelegate.ExecuteIfBound();
	PendingMovementFinishedDelegate.Unbind();
}