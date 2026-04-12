// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Types/StatusEffectTypes.h"
#include "Components/HorizontalBox.h"
#include "UnitHPBarWidget.generated.h"

class UImage;
class ACharacterBase;
class UCombatEventBus;
class UProgressBar;
class UTextBlock;

/**
 * HP bar widget for a single combat unit.
 *
 * This widget is NOT responsible for its own screen-space positioning.
 * The BattleHUDWidget owns a CanvasPanel and updates each bar's slot
 * position every tick via UpdateScreenPosition().
 *
 * This widget is responsible for:
 *   - Displaying the HP bar and text
 *   - Subscribing to CombatEventBus and updating on damage/heal/death
 *   - Notifying Blueprint when status effects are applied or cleared
 *     so the BP can stack/remove icon widgets dynamically
 *   - Exposing BlueprintImplementableEvents for visual feedback
 */
UCLASS(Abstract)
class NEVERGONE_API UUnitHPBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, Category = "HP Bar")
    void InitializeForUnit(ACharacterBase* InUnit, UCombatEventBus* InEventBus,
                            int32 MaxHP, int32 CurrentHP);

    /**
     * Populates status icons for effects that were already active when this widget
     * was created (mid-combat load). Must be called after InitializeForUnit.
     * Iterates the provided list and fires OnStatusIconAdded once per unique tag,
     * mirroring what would have happened had the events been broadcast in order.
     */
    void SyncInitialStatusIcons(const TArray<struct FActiveStatusEffect>& ActiveEffects);

    UFUNCTION(BlueprintCallable, Category = "HP Bar")
    void Deinitialize();

    UFUNCTION(BlueprintPure, Category = "HP Bar")
    ACharacterBase* GetTrackedUnit() const { return TrackedUnit; }

protected:

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> HPBar;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> HPText;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TMap<FGameplayTag, UHorizontalBox*> IconSlotMap;

    // -----------------------------------------------------------------------
    // HP visual events (implement animations in Blueprint)
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintImplementableEvent, Category = "HP Bar")
    void OnDamageReceived(int32 NewHP, int32 OldHP);

    UFUNCTION(BlueprintImplementableEvent, Category = "HP Bar")
    void OnHealReceived(int32 NewHP, int32 OldHP);

    UFUNCTION(BlueprintImplementableEvent, Category = "HP Bar")
    void OnUnitDied();

    // -----------------------------------------------------------------------
    // Status effect icon events (implement stacking icon display in Blueprint)
    //
    // The Blueprint should maintain a horizontal/wrap box of icon images.
    // On OnStatusIconAdded: spawn/show an icon widget using the provided Icon texture.
    // On OnStatusIconRemoved: find and remove the icon widget for that StatusTag.
    // Multiple statuses can be active simultaneously — each StatusTag is unique per unit.
    // -----------------------------------------------------------------------

    /**
     * Called when a new status is applied to this unit.
     * @param StatusTag   Gameplay tag of the applied status (use as key to find/remove later).
     * @param Icon        Texture for the icon. May be null — hide or use a placeholder.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Status Icons")
    void OnStatusIconAdded(FGameplayTag StatusTag, UTexture2D* Icon);

    /**
     * Called when a status is fully removed from this unit (all stacks gone).
     * @param StatusTag   Gameplay tag of the removed status.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Status Icons")
    void OnStatusIconRemoved(FGameplayTag StatusTag);

private:

    void HandleDamageApplied(ACharacterBase* Source, ACharacterBase* Target, int32 Amount);
    void HandleHealApplied(ACharacterBase* Source, ACharacterBase* Target, int32 Amount);
    void HandleUnitDied(ACharacterBase* Unit);
    void HandleStatusApplied(ACharacterBase* Target, const FGameplayTag& StatusTag, UTexture2D* Icon);
    void HandleStatusCleared(ACharacterBase* Target, const FGameplayTag& StatusTag);

    // All HP values are int32 — no fractional HP exists in this system
    void RefreshDisplay(int32 NewHP);

    UPROPERTY()
    TObjectPtr<ACharacterBase> TrackedUnit;

    UPROPERTY()
    TObjectPtr<UCombatEventBus> EventBus;

    int32 CachedMaxHP     = 1;
    int32 CachedCurrentHP = 1;

    FDelegateHandle DamageHandle;
    FDelegateHandle HealHandle;
    FDelegateHandle DiedHandle;
    FDelegateHandle StatusAppliedHandle;
    FDelegateHandle StatusClearedHandle;
};