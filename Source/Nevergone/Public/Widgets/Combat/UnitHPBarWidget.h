// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnitHPBarWidget.generated.h"

class ACharacterBase;
class UCombatEventBus;
class UProgressBar;
class UTextBlock;

/**
 * Persistent HP bar widget anchored above a single combat unit.
 *
 * Lifecycle:
 *   1. Created by UBattleHUDWidget::SpawnHPBars once per unit.
 *   2. InitializeForUnit sets the tracked unit and subscribes to the event bus.
 *   3. Tick updates the widget's screen position to follow the unit.
 *   4. Deinitialize unsubscribes before RemoveFromParent.
 *
 * Visual flash on damage/heal:
 *   The Blueprint subclass implements OnDamageReceived / OnHealReceived
 *   to play a short animation (e.g. red flash, green pulse) without any
 *   C++ animation code here.
 */
UCLASS(Abstract)
class NEVERGONE_API UUnitHPBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/**
	 * Binds this bar to a specific unit and the combat event bus.
	 *
	 * @param InUnit      The character this bar tracks
	 * @param InEventBus  The bus that fires damage/heal events
	 * @param MaxHP       Starting max HP — used to compute fill ratio
	 * @param CurrentHP   Starting current HP
	 */
	UFUNCTION(BlueprintCallable, Category = "HP Bar")
	void InitializeForUnit(ACharacterBase* InUnit, UCombatEventBus* InEventBus,
	                        float MaxHP, float CurrentHP);

	/** Removes event bus subscriptions. Call before RemoveFromParent. */
	UFUNCTION(BlueprintCallable, Category = "HP Bar")
	void Deinitialize();

	/** Returns the character this bar is tracking. */
	UFUNCTION(BlueprintPure, Category = "HP Bar")
	ACharacterBase* GetTrackedUnit() const { return TrackedUnit; }

protected:

	// -----------------------------------------------------------------------
	// Named UMG widgets — bind in the Blueprint designer
	// -----------------------------------------------------------------------

	/** Progress bar showing HP ratio (0..1). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HPBar;

	/** Optional text showing "CurrentHP / MaxHP". */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HPText;

	// -----------------------------------------------------------------------
	// Blueprint events — implement animations in the Blueprint subclass
	// -----------------------------------------------------------------------

	/** Called after HP is updated following a damage event. */
	UFUNCTION(BlueprintImplementableEvent, Category = "HP Bar")
	void OnDamageReceived(float NewHP, float OldHP);

	/** Called after HP is updated following a heal event. */
	UFUNCTION(BlueprintImplementableEvent, Category = "HP Bar")
	void OnHealReceived(float NewHP, float OldHP);

	/** Called when the unit's HP reaches zero. */
	UFUNCTION(BlueprintImplementableEvent, Category = "HP Bar")
	void OnUnitDied();

	// -----------------------------------------------------------------------
	// Tick — keeps widget anchored above the unit's head
	// -----------------------------------------------------------------------

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// -----------------------------------------------------------------------
	// Configurable offset
	// -----------------------------------------------------------------------

	/**
	 * World-space Z offset above the actor origin where the bar appears.
	 * Increase this if your character capsules are tall.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "HP Bar|Layout")
	float WorldZOffset = 200.f;

private:

	// -----------------------------------------------------------------------
	// Event bus handlers
	// -----------------------------------------------------------------------

	void HandleDamageApplied(ACharacterBase* Source, ACharacterBase* Target, float Amount);
	void HandleHealApplied(ACharacterBase* Source, ACharacterBase* Target, float Amount);
	void HandleUnitDied(ACharacterBase* Unit);

	/** Updates HPBar percent and HPText label. */
	void RefreshDisplay(float NewHP);

	// -----------------------------------------------------------------------
	// Private state
	// -----------------------------------------------------------------------

	UPROPERTY()
	TObjectPtr<ACharacterBase> TrackedUnit;

	UPROPERTY()
	TObjectPtr<UCombatEventBus> EventBus;

	float CachedMaxHP   = 1.f;
	float CachedCurrentHP = 1.f;

	FDelegateHandle DamageHandle;
	FDelegateHandle HealHandle;
	FDelegateHandle DiedHandle;
};