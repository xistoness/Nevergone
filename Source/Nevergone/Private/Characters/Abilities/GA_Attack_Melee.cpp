// Copyright Xyzto Works

#include "Characters/Abilities/GA_Attack_Melee.h"

#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/AbilityPreview/Preview_Attack_Melee.h"
#include "Characters/Abilities/BattleMovementAbility.h"
#include "Characters/Abilities/TargetingRules/FactionRule.h"
#include "Level/GridManager.h"
#include "Types/BattleTypes.h"

UGA_Attack_Melee::UGA_Attack_Melee()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Melee")));
	SetAssetTags(AssetTags);

	ActivationRequiredTags.AddTag(
		FGameplayTag::RequestGameplayTag(FName("Mode.Combat"))
	);

	ActivationRequiredTags.AddTag(
		FGameplayTag::RequestGameplayTag(FName("Turn.CanAct"))
	);

	ActivationBlockedTags.AddTag(
		FGameplayTag::RequestGameplayTag(FName("State.Stunned"))
	);

	TargetPolicy.AddRule(MakeUnique<FactionRule>(EFactionType::Enemy));
	
	PreviewRendererClass = UPreview_Attack_Melee::StaticClass();
}

FActionResult UGA_Attack_Melee::BuildPreview(const FActionContext& Context) const
{
	FActionResult Result;
	BuildAttackResult(Context, Result);
	return Result;
}

FActionResult UGA_Attack_Melee::BuildExecution(const FActionContext& Context) const
{
	FActionResult Result;
	BuildAttackResult(Context, Result);
	return Result;
}

bool UGA_Attack_Melee::BuildAttackResult(const FActionContext& Context, FActionResult& OutResult) const
{
	OutResult = FActionResult();

	ACharacterBase* SourceCharacter = Cast<ACharacterBase>(Context.SourceActor);
	ACharacterBase* TargetCharacter = Cast<ACharacterBase>(Context.TargetActor);
	
	// If the approach tile is the same as source tile, path may be empty or trivial. That's okay.
	if (!SourceCharacter || !TargetCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Attack_Melee]: No SourceCharacter or NoTargetCharacter"));
		return false;
	}

	if (!IsTargetValidForMelee(SourceCharacter, TargetCharacter))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Attack_Melee]: Target is not valid for melee attack"));
		return false;
	}

	UUnitStatsComponent* SourceStats = SourceCharacter->GetUnitStats();
	UUnitStatsComponent* TargetStats = TargetCharacter->GetUnitStats();

	if (!SourceStats || !TargetStats)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Attack_Melee]: No SourceStats or no TargetStats"));
		return false;
	}

	if (!TargetStats->IsAlive())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Attack_Melee]: Target is not alive"));
		return false;
	}

	if (IsAdjacent(Context.SourceGridCoord, Context.HoveredGridCoord))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Attack_Melee]: Final tile is adjacent"));
		OutResult.bIsValid = true;
		OutResult.bRequiresMovement = false;
		OutResult.MovementTargetGridCoord = Context.SourceGridCoord;
		OutResult.MovementTargetWorldPosition = SourceCharacter->GetActorLocation();
		OutResult.ActionPointsCost = BaseActionPointsCost;
		OutResult.ExpectedDamage = SourceStats->GetMeleeAttack();
		OutResult.HitChance = 1.0f;
		OutResult.ResolvedAttackTarget = TargetCharacter;
		OutResult.AffectedActors.Add(TargetCharacter);
		return true;
	}

	if (!TryBuildApproachResult(Context, SourceCharacter, TargetCharacter, OutResult))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Attack_Melee]: Couldn't build approach result"));
		return false;
	}

	OutResult.ExpectedDamage = SourceStats->GetMeleeAttack();
	OutResult.HitChance = 1.0f;
	OutResult.ResolvedAttackTarget = TargetCharacter;
	OutResult.AffectedActors.Add(TargetCharacter);

	return OutResult.bIsValid;
}

bool UGA_Attack_Melee::TryBuildApproachResult(
	const FActionContext& Context,
	ACharacterBase* SourceCharacter,
	ACharacterBase* TargetCharacter,
	FActionResult& OutResult
) const
{
	if (!SourceCharacter || !TargetCharacter)
	{
		return false;
	}

	UWorld* World = SourceCharacter->GetWorld();
	if (!World)
	{
		return false;
	}

	UGridManager* Grid = World->GetSubsystem<UGridManager>();
	if (!Grid)
	{
		return false;
	}

	UBattleModeComponent* BattleMode = SourceCharacter->GetBattleModeComponent();
	if (!BattleMode)
	{
		return false;
	}

	const TSubclassOf<UBattleMovementAbility> MovementClass =
		BattleMode->GetEquippedMovementAbilityClass();

	if (!MovementClass)
	{
		return false;
	}

	const UBattleMovementAbility* MovementCDO =
		MovementClass->GetDefaultObject<UBattleMovementAbility>();

	if (!MovementCDO)
	{
		return false;
	}

	const TArray<FIntPoint> AdjacentCoords = GetAdjacentCoords(Context.HoveredGridCoord);

	bool bFoundAny = false;
	FActionResult BestMovementResult;
	int32 BestPathLength = TNumericLimits<int32>::Max();

	for (const FIntPoint& CandidateCoord : AdjacentCoords)
	{
		FActionContext MovementContext{};
		MovementContext.SourceActor = Context.SourceActor;
		MovementContext.TargetActor = Grid->GetActorAt(CandidateCoord);
		MovementContext.SourceGridCoord = Context.SourceGridCoord;
		MovementContext.HoveredGridCoord = CandidateCoord;
		MovementContext.MovementTargetGridCoord = CandidateCoord;
		MovementContext.SourceWorldPosition = Context.SourceWorldPosition;
		MovementContext.HoveredWorldPosition = Grid->GridToWorld(CandidateCoord);
		MovementContext.MovementTargetWorldPosition = Grid->GridToWorld(CandidateCoord);
		MovementContext.TraversalParams = Context.TraversalParams;
		MovementContext.CachedPathCost = Grid->CalculatePathCost(
			Context.SourceGridCoord,
			CandidateCoord,
			Context.TraversalParams
		);

		if (MovementContext.CachedPathCost == INDEX_NONE || MovementContext.CachedPathCost < 0)
		{
			continue;
		}

		const FActionResult MovementResult = MovementCDO->BuildMovementExecution(MovementContext);
		if (!MovementResult.bIsValid)
		{
			continue;
		}

		const int32 TotalAPCost = FMath::Max(BaseActionPointsCost, MovementResult.ActionPointsCost);
		const int32 PathLength = MovementResult.PathPoints.Num();

		if (!bFoundAny
			|| TotalAPCost < OutResult.ActionPointsCost
			|| (TotalAPCost == OutResult.ActionPointsCost && PathLength < BestPathLength))
		{
			bFoundAny = true;
			BestPathLength = PathLength;

			OutResult = MovementResult;
			OutResult.bIsValid = true;
			OutResult.bRequiresMovement = true;
			OutResult.ActionPointsCost = TotalAPCost;
			OutResult.ResolvedAttackTarget = TargetCharacter;
		}
	}

	return bFoundAny;
}

bool UGA_Attack_Melee::IsTargetValidForMelee(
	ACharacterBase* SourceCharacter,
	ACharacterBase* TargetCharacter
) const
{
	if (!SourceCharacter || !TargetCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Attack_Melee]: Invalid SourceCharacter or TargetCharacter"));
		return false;
	}

	FActionContext ValidationContext{};
	ValidationContext.SourceActor = SourceCharacter;
	ValidationContext.TargetActor = TargetCharacter;

	UGridManager* Grid = SourceCharacter->GetWorld()
		? SourceCharacter->GetWorld()->GetSubsystem<UGridManager>()
		: nullptr;

	if (Grid)
	{
		ValidationContext.SourceGridCoord = Grid->WorldToGrid(SourceCharacter->GetActorLocation());
		ValidationContext.HoveredGridCoord = Grid->WorldToGrid(TargetCharacter->GetActorLocation());
	}

	return TargetPolicy.IsValid(ValidationContext);
}

bool UGA_Attack_Melee::IsAdjacent(const FIntPoint& Source, const FIntPoint& Target) const
{
	const int32 Distance = FMath::Max(
		FMath::Abs(Source.X - Target.X),
		FMath::Abs(Source.Y - Target.Y)
	);

	return Distance <= MaxRange;
}

TArray<FIntPoint> UGA_Attack_Melee::GetAdjacentCoords(const FIntPoint& Center) const
{
	TArray<FIntPoint> Result;
	Result.Reserve(8);

	for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
	{
		for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
		{
			if (OffsetX == 0 && OffsetY == 0)
			{
				continue;
			}

			Result.Add(FIntPoint(Center.X + OffsetX, Center.Y + OffsetY));
		}
	}

	return Result;
}

void UGA_Attack_Melee::ActivateAbility(
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

	const FActionContext& Context = BattleMode->GetCurrentContext();
	const FActionResult ExecutionResult = BuildExecution(Context);

	if (!ExecutionResult.bIsValid)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedCharacter = SourceCharacter;
	CachedTargetCharacter = Cast<ACharacterBase>(ExecutionResult.ResolvedAttackTarget);
	CachedActionPointsCost = ExecutionResult.ActionPointsCost;
	bCachedRequiredMovement = ExecutionResult.bRequiresMovement;
	CachedFinalGridCoord = ExecutionResult.bRequiresMovement
		? ExecutionResult.MovementTargetGridCoord
		: Context.SourceGridCoord;

	if (!CachedTargetCharacter)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!ExecutionResult.bRequiresMovement)
	{
		ApplyResolvedAttack();
		FinalizeAttackAction();
		return;
	}

	const TSubclassOf<UBattleMovementAbility> MovementClass =
		BattleMode->GetEquippedMovementAbilityClass();

	if (!MovementClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveMovementExecutor = NewObject<UBattleMovementAbility>(this, MovementClass);
	if (!ActiveMovementExecutor)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FSimpleDelegate MovementFinishedDelegate;
	MovementFinishedDelegate.BindUObject(this, &UGA_Attack_Melee::HandleApproachMovementFinished);

	if (!ActiveMovementExecutor->ExecuteMovementPlan(
		SourceCharacter,
		ExecutionResult,
		MovementFinishedDelegate))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UGA_Attack_Melee::HandleApproachMovementFinished()
{
	ApplyResolvedAttack();
	FinalizeAttackAction();
}

void UGA_Attack_Melee::ApplyResolvedAttack()
{
	if (!CachedCharacter || !CachedTargetCharacter)
	{
		return;
	}

	UUnitStatsComponent* SourceStats = CachedCharacter->GetUnitStats();
	UUnitStatsComponent* TargetStats = CachedTargetCharacter->GetUnitStats();

	if (!SourceStats || !TargetStats)
	{
		return;
	}

	if (!TargetStats->IsAlive())
	{
		return;
	}

	const float Damage = SourceStats->GetMeleeAttack();
	TargetStats->SetCurrentHP(TargetStats->GetCurrentHP() - Damage);
}

void UGA_Attack_Melee::FinalizeAttackAction()
{
	FinalizeAbilityExecution();
}

void UGA_Attack_Melee::ApplyAbilityCompletionEffects()
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

	if (bCachedRequiredMovement)
	{
		BattleMode->UpdateOccupancy(CachedFinalGridCoord);
	}

	BattleMode->ConsumeActionPoints(CachedActionPointsCost);
	
}

void UGA_Attack_Melee::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	ActiveMovementExecutor = nullptr;
	CachedCharacter = nullptr;
	CachedTargetCharacter = nullptr;
	CachedActionPointsCost = 0;
	bCachedRequiredMovement = false;
	CachedFinalGridCoord = FIntPoint::ZeroValue;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}