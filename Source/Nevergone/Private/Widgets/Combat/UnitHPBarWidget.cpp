// Copyright Xyzto Works

#include "Widgets/Combat/UnitHPBarWidget.h"

#include "Characters/CharacterBase.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameMode/Combat/CombatEventBus.h"

void UUnitHPBarWidget::InitializeForUnit(ACharacterBase* InUnit, UCombatEventBus* InEventBus,
                                          int32 MaxHP, int32 CurrentHP)
{
	if (!InUnit || !InEventBus)
	{
		UE_LOG(LogTemp, Error, TEXT("[UnitHPBarWidget] InitializeForUnit: null Unit or EventBus"));
		return;
	}

	TrackedUnit       = InUnit;
	EventBus          = InEventBus;
	CachedMaxHP       = FMath::Max(1, MaxHP);
	CachedCurrentHP   = CurrentHP;

	DamageHandle        = InEventBus->OnDamageApplied.AddUObject(this, &UUnitHPBarWidget::HandleDamageApplied);
	HealHandle          = InEventBus->OnHealApplied.AddUObject(this,   &UUnitHPBarWidget::HandleHealApplied);
	DiedHandle          = InEventBus->OnUnitDied.AddUObject(this,      &UUnitHPBarWidget::HandleUnitDied);
	StatusAppliedHandle = InEventBus->OnStatusApplied.AddUObject(this, &UUnitHPBarWidget::HandleStatusApplied);
	StatusClearedHandle = InEventBus->OnStatusCleared.AddUObject(this, &UUnitHPBarWidget::HandleStatusCleared);

	RefreshDisplay(CachedCurrentHP);

	UE_LOG(LogTemp, Log, TEXT("[UnitHPBarWidget] Initialized for %s (HP %d/%d)"),
		*GetNameSafe(TrackedUnit), CachedCurrentHP, CachedMaxHP);
}

void UUnitHPBarWidget::Deinitialize()
{
	if (EventBus)
	{
		if (DamageHandle.IsValid())        { EventBus->OnDamageApplied.Remove(DamageHandle);   DamageHandle.Reset();        }
		if (HealHandle.IsValid())          { EventBus->OnHealApplied.Remove(HealHandle);         HealHandle.Reset();          }
		if (DiedHandle.IsValid())          { EventBus->OnUnitDied.Remove(DiedHandle);             DiedHandle.Reset();          }
		if (StatusAppliedHandle.IsValid()) { EventBus->OnStatusApplied.Remove(StatusAppliedHandle); StatusAppliedHandle.Reset(); }
		if (StatusClearedHandle.IsValid()) { EventBus->OnStatusCleared.Remove(StatusClearedHandle); StatusClearedHandle.Reset(); }
	}

	UE_LOG(LogTemp, Log, TEXT("[UnitHPBarWidget] Deinitialized for %s"), *GetNameSafe(TrackedUnit));

	TrackedUnit = nullptr;
	EventBus    = nullptr;
}

// ---------------------------------------------------------------------------
// Event bus handlers
// ---------------------------------------------------------------------------

void UUnitHPBarWidget::HandleDamageApplied(ACharacterBase* Source, ACharacterBase* Target, int32 Amount)
{
	if (Target != TrackedUnit) { return; }

	const int32 OldHP = CachedCurrentHP;
	CachedCurrentHP   = FMath::Max(0, CachedCurrentHP - Amount);

	RefreshDisplay(CachedCurrentHP);
	OnDamageReceived(CachedCurrentHP, OldHP);

	UE_LOG(LogTemp, Log, TEXT("[UnitHPBarWidget] %s took %d damage — HP %d/%d"),
		*GetNameSafe(TrackedUnit), Amount, CachedCurrentHP, CachedMaxHP);
}

void UUnitHPBarWidget::HandleHealApplied(ACharacterBase* Source, ACharacterBase* Target, int32 Amount)
{
	if (Target != TrackedUnit) { return; }

	const int32 OldHP = CachedCurrentHP;
	CachedCurrentHP   = FMath::Min(CachedMaxHP, CachedCurrentHP + Amount);

	RefreshDisplay(CachedCurrentHP);
	OnHealReceived(CachedCurrentHP, OldHP);

	UE_LOG(LogTemp, Log, TEXT("[UnitHPBarWidget] %s healed %d — HP %d/%d"),
		*GetNameSafe(TrackedUnit), Amount, CachedCurrentHP, CachedMaxHP);
}

void UUnitHPBarWidget::HandleUnitDied(ACharacterBase* Unit)
{
	if (Unit != TrackedUnit) { return; }

	CachedCurrentHP = 0;
	RefreshDisplay(0);
	OnUnitDied();

	// Hide the bar — BattleHUDWidget NativeTick keeps it hidden via bIsDead check
	SetVisibility(ESlateVisibility::Hidden);

	UE_LOG(LogTemp, Log, TEXT("[UnitHPBarWidget] %s died — bar hidden"), *GetNameSafe(TrackedUnit));
}

void UUnitHPBarWidget::HandleStatusApplied(ACharacterBase* Target, const FGameplayTag& StatusTag, UTexture2D* Icon)
{
	if (Target != TrackedUnit) { return; }

	UE_LOG(LogTemp, Log, TEXT("[UnitHPBarWidget] %s received status '%s' — notifying BP for icon"),
		*GetNameSafe(TrackedUnit), *StatusTag.ToString());

	// Delegate to Blueprint to create/show the icon widget.
	// Blueprint should maintain a map of StatusTag -> icon widget so it can
	// remove the right one on OnStatusIconRemoved.
	OnStatusIconAdded(StatusTag, Icon);
}

void UUnitHPBarWidget::HandleStatusCleared(ACharacterBase* Target, const FGameplayTag& StatusTag)
{
	if (Target != TrackedUnit) { return; }

	UE_LOG(LogTemp, Log, TEXT("[UnitHPBarWidget] %s status '%s' cleared — notifying BP to remove icon"),
		*GetNameSafe(TrackedUnit), *StatusTag.ToString());

	OnStatusIconRemoved(StatusTag);
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void UUnitHPBarWidget::RefreshDisplay(int32 NewHP)
{
	const float Percent = CachedMaxHP > 0
		? FMath::Clamp(static_cast<float>(NewHP) / static_cast<float>(CachedMaxHP), 0.f, 1.f)
		: 0.f;

	if (HPBar)
	{
		HPBar->SetPercent(Percent);
	}

	if (HPText)
	{
		HPText->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d"), NewHP, CachedMaxHP)));
	}
}