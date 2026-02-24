// Copyright Xyzto Works


#include "Characters/Abilities/GA_Move_Simple.h"

#include "StateTreePropertyBindings.h"
#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/AbilityPreview/Preview_Move.h"
#include "Characters/Abilities/TargetingRules/MoveRangeRule.h"
#include "Characters/Abilities/TargetingRules/TileNotBlockedRule.h"
#include "Characters/Abilities/TargetingRules/TileNotOccupiedRule.h"
#include "Level/GridManager.h"
#include "Types/BattleTypes.h"

UGA_Move_Simple::UGA_Move_Simple()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	FGameplayTagContainer Tags;
	Tags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Move"));
	SetAssetTags(Tags);
	
	TargetPolicy.AddRule(MakeUnique<MoveRangeRule>());
	TargetPolicy.AddRule(MakeUnique<TileNotBlockedRule>());
	TargetPolicy.AddRule(MakeUnique<TileNotOccupiedRule>());
	PreviewRendererClass = UPreview_Move::StaticClass();
}

FActionResult UGA_Move_Simple::BuildPreview(const FActionContext& Context) const
{
	UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: Trying to build preview..."));
	FActionResult Result;

	ACharacterBase* Character =
		Cast<ACharacterBase>(Context.SourceActor);

	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: Couldn't find Character..."));
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
	
	UGridManager* Grid =
		World->GetSubsystem<UGridManager>();

	if (!Grid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: Grid is invalid..."));
		return Result;
	}

	FIntPoint TargetCoord =	Grid->WorldToGrid(Context.TargetWorldPosition);

	const FGridTile* Tile =	Grid->GetTile(TargetCoord);
	
	int32 Distance = Context.CachedPathCost; // already calculated in tiles
	int32 Speed = Character->GetUnitStats()->GetSpeed();

	int32 ActionPointsCost = (Distance + Speed - 1) / Speed;

	Result.bIsValid = true;
	Result.TargetGridCoord = TargetCoord;
	Result.TargetWorldPosition = Tile->WorldLocation;
	Result.ActionPointsCost = ActionPointsCost;

	Character->SetPendingMoveLocation(Result.TargetWorldPosition);

	return Result;
}

void UGA_Move_Simple::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                      const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                      const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: Ability activated..."));
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	ACharacterBase* Character =
		Cast<ACharacterBase>(ActorInfo->AvatarActor.Get());

	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	FVector TargetLocation = Character->GetPendingMoveLocation();

	Character->MoveToLocation(TargetLocation);
	
	Character->GetBattleModeComponent()->UpdateOccupancy();
	Character->GetBattleModeComponent()->ConsumeActionPoints();
	
	UE_LOG(LogTemp, Warning, TEXT("[MoveSimple]: Ability reached end!"));

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
