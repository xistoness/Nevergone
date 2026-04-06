// Copyright Xyzto Works

#include "GameMode/Combat/CombatEventBus.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "GameMode/Combat/BattleState.h"
#include "GameMode/Combat/StatusEffectManager.h"
#include "Widgets/Combat/FloatingCombatTextWidget.h"

void UCombatEventBus::Initialize(UBattleState* InBattleState)
{
	BattleState = InBattleState;
}

void UCombatEventBus::SetStatusEffectManager(UStatusEffectManager* InManager)
{
	StatusEffectManager = InManager;
}

void UCombatEventBus::NotifyDamageApplied(
	ACharacterBase* Source,
	ACharacterBase* Target,
	int32           Amount)
{
	if (!Target || Amount <= 0)
	{
		return;
	}

	UUnitStatsComponent* TargetStats = Target->GetUnitStats();
	if (!TargetStats || !TargetStats->IsAlive())
	{
		return;
	}

	// Run incoming damage through any active Shield layers first.
	// AbsorbDamageWithShield returns the remaining damage after shields are depleted.
	// If shields absorb everything, Amount becomes 0 and HP is untouched.
	int32 EffectiveAmount = Amount;
	if (StatusEffectManager)
	{
		EffectiveAmount = StatusEffectManager->AbsorbDamageWithShield(Target, Amount);
	}

	if (EffectiveAmount <= 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[CombatEventBus] %d damage to %s fully absorbed by shield"),
			Amount, *GetNameSafe(Target));
		return;
	}

	// Mutate the persistent stat component (pure data write, no side effects)
	const int32 ClampedAmount = FMath::Min(EffectiveAmount, TargetStats->GetCurrentHP());
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
		FString::FromInt(ClampedAmount),
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

void UCombatEventBus::NotifyHealApplied(ACharacterBase* Source, ACharacterBase* Target, int32 Amount)
{
	if (!Target || Amount <= 0.f) return;

	UUnitStatsComponent* TargetStats = Target->GetUnitStats();
	if (!TargetStats || !TargetStats->IsAlive()) return;

	const int32 HeadRoom    = TargetStats->GetMaxHP() - TargetStats->GetCurrentHP();
	const int32 ClampedHeal = FMath::Min(Amount, HeadRoom);

	// Apply HP only if there's room — but always show feedback
	if (ClampedHeal > 0)
	{
		TargetStats->SetCurrentHP(TargetStats->GetCurrentHP() + ClampedHeal);

		if (BattleState)
		{
			BattleState->ApplyHeal(Target, ClampedHeal);
		}
	}

	// Always show floating text and broadcast — even if heal was fully clamped
	Target->SpawnFloatingText(
		FString::Printf(TEXT("+%d"), ClampedHeal),
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

	// Trigger floating status text on the target character
	Target->SpawnFloatingText(DisplayLabel, EFloatingTextType::StatusApply, Icon);

	// Pass Icon along so HP bar widgets can display persistent status icons
	OnStatusApplied.Broadcast(Target, StatusTag, Icon);
}

void UCombatEventBus::NotifyStatusCleared(
	ACharacterBase*     Target,
	const FGameplayTag& StatusTag)
{
	if (!Target)
	{
		return;
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