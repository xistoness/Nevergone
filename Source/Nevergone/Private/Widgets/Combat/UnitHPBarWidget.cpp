// Copyright Xyzto Works

#include "Widgets/Combat/UnitHPBarWidget.h"

#include "Characters/CharacterBase.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameMode/Combat/CombatEventBus.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet/GameplayStatics.h"

void UUnitHPBarWidget::InitializeForUnit(ACharacterBase* InUnit, UCombatEventBus* InEventBus,
                                          float MaxHP, float CurrentHP)
{
	if (!InUnit || !InEventBus)
	{
		UE_LOG(LogTemp, Error, TEXT("[UnitHPBarWidget] InitializeForUnit: null Unit or EventBus"));
		return;
	}

	TrackedUnit      = InUnit;
	EventBus         = InEventBus;
	CachedMaxHP      = FMath::Max(1.f, MaxHP);
	CachedCurrentHP  = CurrentHP;

	// Subscribe to the event bus — only update when our unit is involved
	DamageHandle = InEventBus->OnDamageApplied.AddUObject(this, &UUnitHPBarWidget::HandleDamageApplied);
	HealHandle   = InEventBus->OnHealApplied.AddUObject(this,   &UUnitHPBarWidget::HandleHealApplied);
	DiedHandle   = InEventBus->OnUnitDied.AddUObject(this,      &UUnitHPBarWidget::HandleUnitDied);

	RefreshDisplay(CachedCurrentHP);

	UE_LOG(LogTemp, Log, TEXT("[UnitHPBarWidget] Initialized for %s (HP %.0f/%.0f)"),
		*GetNameSafe(TrackedUnit), CachedCurrentHP, CachedMaxHP);
}

void UUnitHPBarWidget::Deinitialize()
{
	if (EventBus)
	{
		if (DamageHandle.IsValid()) { EventBus->OnDamageApplied.Remove(DamageHandle); DamageHandle.Reset(); }
		if (HealHandle.IsValid())   { EventBus->OnHealApplied.Remove(HealHandle);     HealHandle.Reset();   }
		if (DiedHandle.IsValid())   { EventBus->OnUnitDied.Remove(DiedHandle);         DiedHandle.Reset();   }
	}

	UE_LOG(LogTemp, Log, TEXT("[UnitHPBarWidget] Deinitialized for %s"), *GetNameSafe(TrackedUnit));

	TrackedUnit = nullptr;
	EventBus    = nullptr;
}

// ---------------------------------------------------------------------------
// NativeTick — project world position to screen and reposition widget
// ---------------------------------------------------------------------------

void UUnitHPBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsValid(TrackedUnit))
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	// Compute the world position above the unit's origin
	const FVector WorldPos = TrackedUnit->GetActorLocation() + FVector(0.f, 0.f, WorldZOffset);

	// Project to screen space
	FVector2D ScreenPos;
	if (!PC->ProjectWorldLocationToScreen(WorldPos, ScreenPos, true))
	{
		// Unit is behind camera — hide the bar
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Convert from pixel coordinates to DPI-scaled viewport coordinates
	const float DPIScale = UWidgetLayoutLibrary::GetViewportScale(this);
	ScreenPos /= DPIScale;

	// Center the widget horizontally above the unit
	const FVector2D WidgetSize = MyGeometry.GetLocalSize();
	ScreenPos.X -= WidgetSize.X * 0.5f;

	// Reposition in viewport — HP bars are added directly to the viewport (no parent panel),
	// so we use SetPositionInViewport which works regardless of slot type.
	SetPositionInViewport(ScreenPos, false);
}

// ---------------------------------------------------------------------------
// Event bus handlers
// ---------------------------------------------------------------------------

void UUnitHPBarWidget::HandleDamageApplied(ACharacterBase* Source, ACharacterBase* Target, float Amount)
{
	if (Target != TrackedUnit)
	{
		return;
	}

	const float OldHP = CachedCurrentHP;
	CachedCurrentHP   = FMath::Max(0.f, CachedCurrentHP - Amount);

	RefreshDisplay(CachedCurrentHP);
	OnDamageReceived(CachedCurrentHP, OldHP);

	UE_LOG(LogTemp, Log, TEXT("[UnitHPBarWidget] %s took %.1f damage — HP %.0f/%.0f"),
		*GetNameSafe(TrackedUnit), Amount, CachedCurrentHP, CachedMaxHP);
}

void UUnitHPBarWidget::HandleHealApplied(ACharacterBase* Source, ACharacterBase* Target, float Amount)
{
	if (Target != TrackedUnit)
	{
		return;
	}

	const float OldHP = CachedCurrentHP;
	CachedCurrentHP   = FMath::Min(CachedMaxHP, CachedCurrentHP + Amount);

	RefreshDisplay(CachedCurrentHP);
	OnHealReceived(CachedCurrentHP, OldHP);

	UE_LOG(LogTemp, Log, TEXT("[UnitHPBarWidget] %s healed %.1f — HP %.0f/%.0f"),
		*GetNameSafe(TrackedUnit), Amount, CachedCurrentHP, CachedMaxHP);
}

void UUnitHPBarWidget::HandleUnitDied(ACharacterBase* Unit)
{
	if (Unit != TrackedUnit)
	{
		return;
	}

	CachedCurrentHP = 0.f;
	RefreshDisplay(0.f);
	OnUnitDied();

	UE_LOG(LogTemp, Log, TEXT("[UnitHPBarWidget] %s died — bar set to zero"), *GetNameSafe(TrackedUnit));
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void UUnitHPBarWidget::RefreshDisplay(float NewHP)
{
	const float Percent = CachedMaxHP > 0.f ? (NewHP / CachedMaxHP) : 0.f;

	if (HPBar)
	{
		HPBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
	}

	if (HPText)
	{
		HPText->SetText(FText::FromString(
			FString::Printf(TEXT("%.0f / %.0f"), NewHP, CachedMaxHP)));
	}
}