// Copyright Xyzto Works

#include "GameMode/Combat/CombatEventBus.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "GameMode/Combat/BattleState.h"
#include "Widgets/Combat/FloatingCombatTextWidget.h"

void UCombatEventBus::Initialize(UBattleState* InBattleState)
{
	BattleState = InBattleState;
}

void UCombatEventBus::NotifyDamageApplied(
	ACharacterBase* Source,
	ACharacterBase* Target,
	float           Amount)
{
	if (!Target || Amount <= 0.f)
	{
		return;
	}

	UUnitStatsComponent* TargetStats = Target->GetUnitStats();
	if (!TargetStats || !TargetStats->IsAlive())
	{
		return;
	}

	// Mutate the persistent stat component (pure data write, no side effects)
	const float ClampedAmount = FMath::Min(Amount, TargetStats->GetCurrentHP());
	TargetStats->SetCurrentHP(TargetStats->GetCurrentHP() - ClampedAmount);

	// Keep BattleState in sync
	if (BattleState)
	{
		BattleState->ApplyDamage(Target, ClampedAmount);
	}

	// Trigger a brief red flash on the target mesh to give visual feedback for the hit.
	// This works without animations by temporarily overriding the emissive parameter.
	FlashHitEffect(Target);

	// Trigger floating combat text on the target character
	Target->SpawnFloatingText(
		FString::FromInt(FMath::RoundToInt(ClampedAmount)),
		EFloatingTextType::Damage
	);

	// Broadcast to all other listeners (audio, animations, UI bars, etc.)
	OnDamageApplied.Broadcast(Source, Target, ClampedAmount);

	// Check death after all listeners have processed the damage event
	if (!TargetStats->IsAlive())
	{
		OnUnitDied.Broadcast(Target);
	}
}

void UCombatEventBus::NotifyHealApplied(ACharacterBase* Source, ACharacterBase* Target, float Amount)
{
	if (!Target || Amount <= 0.f) return;

	UUnitStatsComponent* TargetStats = Target->GetUnitStats();
	if (!TargetStats || !TargetStats->IsAlive()) return;

	const float HeadRoom    = TargetStats->GetMaxHP() - TargetStats->GetCurrentHP();
	const float ClampedHeal = FMath::Min(Amount, HeadRoom);

	// Apply HP only if there's room — but always show feedback
	if (ClampedHeal > 0.f)
	{
		TargetStats->SetCurrentHP(TargetStats->GetCurrentHP() + ClampedHeal);

		if (BattleState)
		{
			BattleState->ApplyHeal(Target, ClampedHeal);
		}
	}

	// Always show floating text and broadcast — even if heal was fully clamped
	Target->SpawnFloatingText(
		FString::Printf(TEXT("+%d"), FMath::RoundToInt(ClampedHeal)),
		EFloatingTextType::Heal
	);

	OnHealApplied.Broadcast(Source, Target, ClampedHeal);
}

void UCombatEventBus::NotifyStatusApplied(
	ACharacterBase*     Target,
	const FGameplayTag& StatusTag,
	const FString&      DisplayLabel,
	UTexture2D*         Icon)
{
	if (!Target)
	{
		return;
	}

	// Sync BattleState status tags
	if (BattleState)
	{
		BattleState->ApplyStatusTag(Target, StatusTag);
	}

	// Trigger floating status text on the target character
	Target->SpawnFloatingText(DisplayLabel, EFloatingTextType::StatusApply, Icon);

	OnStatusApplied.Broadcast(Target, StatusTag);
}

void UCombatEventBus::NotifyStatusCleared(
	ACharacterBase*     Target,
	const FGameplayTag& StatusTag)
{
	if (!Target)
	{
		return;
	}

	if (BattleState)
	{
		BattleState->ClearStatusTag(Target, StatusTag);
	}

	OnStatusCleared.Broadcast(Target, StatusTag);
}
// ---------------------------------------------------------------------------
// Hit flash — brief red emissive pulse on the target mesh
// ---------------------------------------------------------------------------

void UCombatEventBus::FlashHitEffect(ACharacterBase* Target)
{
	if (!IsValid(Target))
	{
		return;
	}

	USkeletalMeshComponent* Mesh = Target->GetMesh();
	if (!Mesh)
	{
		return;
	}

	// Apply red emissive tint — the parameter name must match the material.
	// "EmissiveColor" is the convention used in the project's character materials.
	// If your materials use a different name, update HitFlashParamName in the header.
	Mesh->SetVectorParameterValueOnMaterials(HitFlashParamName, FVector(HitFlashColor));

	UWorld* World = Target->GetWorld();
	if (!World)
	{
		return;
	}

	// Schedule reset after flash duration — captured by value so the lambda is safe
	// even if Target is destroyed before the timer fires (IsValid guard inside)
	TWeakObjectPtr<ACharacterBase> WeakTarget(Target);
	const FName ParamName  = HitFlashParamName;
	const float Duration   = HitFlashDurationSeconds;

	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, [WeakTarget, ParamName]()
	{
		if (!WeakTarget.IsValid())
		{
			return;
		}

		USkeletalMeshComponent* TargetMesh = WeakTarget->GetMesh();
		if (TargetMesh)
		{
			// Reset to black (no emissive) — restores the material to its base state
			TargetMesh->SetVectorParameterValueOnMaterials(ParamName, FVector::ZeroVector);
		}
	}, Duration, false);
}