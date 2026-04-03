// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ActionSlot.generated.h"

class UAbilityDefinition;
class UImage;
class UTextBlock;
class UBorder;

/**
 * Represents a single slot in the ActionHotbar.
 *
 * Each slot displays the icon of a bound ability.
 * When the ability is selected, the border color changes to SelectedBorderColor
 * and the ability's DisplayName appears above the slot via AbilityNameText.
 *
 * Bind the following widgets in the Blueprint subclass using exactly these names:
 *   SlotBorder       - UBorder     (wraps the slot background; its BrushColor drives the border tint)
 *   SlotImage        - UImage      (displays the ability icon)
 *   AbilityNameText  - UTextBlock  (label shown above the slot when selected; collapsed otherwise)
 */
UCLASS()
class NEVERGONE_API UActionSlot : public UUserWidget
{
    GENERATED_BODY()

public:

    /**
     * Populates the slot with data from an ability definition.
     * Call this once per slot when the hotbar is initialized for a unit.
     *
     * @param InDefinition  The ability whose icon and name should be displayed.
     *                      Passing nullptr clears the slot visually.
     */
    UFUNCTION(BlueprintCallable, Category = "ActionSlot")
    void SetAbility(const UAbilityDefinition* InDefinition);

    /**
     * Drives the selected/idle visual state of the slot.
     * - Selected: SlotBorder tints to SelectedBorderColor; AbilityNameText becomes visible.
     * - Idle:     SlotBorder tints to IdleBorderColor; AbilityNameText is collapsed.
     *
     * @param bSelected  True = selected state; False = idle state.
     */
    UFUNCTION(BlueprintCallable, Category = "ActionSlot")
    void SetSelected(bool bSelected);

    // -----------------------------------------------------------------------
    // Designer-facing configuration
    // -----------------------------------------------------------------------

    /** Border brush color when this slot is NOT selected. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActionSlot|Style")
    FLinearColor IdleBorderColor = FLinearColor(0.2f, 0.2f, 0.2f, 1.0f);

    /** Border brush color when this slot IS selected. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActionSlot|Style")
    FLinearColor SelectedBorderColor = FLinearColor(1.0f, 0.85f, 0.0f, 1.0f);

    /** Fallback icon shown when the bound ability has no icon assigned. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActionSlot|Style")
    TObjectPtr<UTexture2D> FallbackIcon;

protected:

    virtual void NativePreConstruct() override;

    // -----------------------------------------------------------------------
    // Widget bindings — names must match exactly in the Blueprint layout
    // -----------------------------------------------------------------------

    /**
     * Border widget that frames the slot.
     * UBorder::SetBrushColor() is used to apply the selected/idle tint.
     * In the Blueprint hierarchy: SlotBorder → (SlotImage + AbilityNameText).
     */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> SlotBorder;

    /** Displays the ability's icon texture inside the border. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> SlotImage;

    /**
     * Shows the ability's display name.
     * Visible (SelfHitTestInvisible) when selected; Collapsed when idle.
     * Place this above SlotBorder in the slot's layout panel.
     */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> AbilityNameText;

private:

    /** Updates SlotImage brush from BoundDefinition (or FallbackIcon). */
    void RefreshIcon();

    /** Applies border color and name text visibility based on bIsSelected. */
    void RefreshSelectionVisuals();

    UPROPERTY()
    TObjectPtr<const UAbilityDefinition> BoundDefinition;

    bool bIsSelected = false;
};