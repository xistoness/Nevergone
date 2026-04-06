// Copyright Xyzto Works

#include "Widgets/Combat/ActionSlot.h"

#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Data/AbilityDefinition.h"
#include "Engine/Texture2D.h"
#include "Nevergone.h"

void UActionSlot::NativePreConstruct()
{
    Super::NativePreConstruct();

    // Apply default idle visuals in the editor viewport so designers
    // see the correct state while editing the widget Blueprint.
    RefreshSelectionVisuals();
}

void UActionSlot::SetAbility(const UAbilityDefinition* InDefinition)
{
    UE_LOG(LogTemp, Log,
        TEXT("[ActionSlot] SetAbility: %s"),
        InDefinition ? *InDefinition->DisplayName.ToString() : TEXT("(none)"));

    BoundDefinition = InDefinition;
    RefreshIcon();
}

void UActionSlot::SetSelected(bool bSelected)
{
    if (bIsSelected == bSelected)
    {
        return;
    }

    bIsSelected = bSelected;

    UE_LOG(LogTemp, Verbose,
        TEXT("[ActionSlot] SetSelected: '%s' → %s"),
        BoundDefinition ? *BoundDefinition->DisplayName.ToString() : TEXT("(empty)"),
        bIsSelected ? TEXT("SELECTED") : TEXT("IDLE"));

    RefreshSelectionVisuals();
}

// ---- Private helpers -------------------------------------------------------

void UActionSlot::RefreshIcon()
{
    if (!SlotImage)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ActionSlot] RefreshIcon: SlotImage binding is missing — check the widget Blueprint."));
        return;
    }

    UTexture2D* IconTexture = nullptr;

    if (BoundDefinition && BoundDefinition->Icon)
    {
        IconTexture = BoundDefinition->Icon;
    }
    else if (FallbackIcon)
    {
        IconTexture = FallbackIcon;
    }

    if (IconTexture)
    {
        SlotImage->SetBrushFromTexture(IconTexture, /*bMatchSize=*/false);
        SlotImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    else
    {
        SlotImage->SetVisibility(ESlateVisibility::Hidden);

        UE_LOG(LogTemp, Warning,
            TEXT("[ActionSlot] RefreshIcon: no icon for '%s' and no FallbackIcon set."),
            BoundDefinition ? *BoundDefinition->DisplayName.ToString() : TEXT("(none)"));
    }
}

void UActionSlot::RefreshSelectionVisuals()
{
    // ---- Border tint ----
    // UBorder::SetBrushColor changes the background/frame color of the border widget.
    // This is the correct API — UOverlay does not have a SetBrushColor or equivalent.
    if (SlotBorder)
    {
        const FLinearColor TargetColor = bIsSelected ? SelectedBorderColor : IdleBorderColor;
        SlotBorder->SetBrushColor(TargetColor);
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ActionSlot] RefreshSelectionVisuals: SlotBorder binding is missing — check the widget Blueprint."));
    }

    // ---- Ability name label ----
    if (AbilityNameText)
    {
        if (bIsSelected && BoundDefinition)
        {
            AbilityNameText->SetText(BoundDefinition->DisplayName);
            AbilityNameText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            AbilityNameText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ActionSlot] RefreshSelectionVisuals: AbilityNameText binding is missing — check the widget Blueprint."));
    }
}
void UActionSlot::SetCooldown(bool bOnCooldown, int32 TurnsRemaining)
{
    if (bIsCooldown == bOnCooldown && CooldownTurnsRemaining == TurnsRemaining)
    {
        return;
    }

    bIsCooldown           = bOnCooldown;
    CooldownTurnsRemaining = TurnsRemaining;

    RefreshCooldownVisuals();
}

void UActionSlot::RefreshCooldownVisuals()
{
    // Overlay visibility
    if (CooldownOverlay)
    {
        CooldownOverlay->SetVisibility(
            bIsCooldown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    // Remaining turns count
    if (CooldownText)
    {
        if (bIsCooldown && CooldownTurnsRemaining > 0)
        {
            CooldownText->SetText(FText::AsNumber(CooldownTurnsRemaining));
            CooldownText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            CooldownText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Dim the icon when on cooldown so the slot reads as unavailable
    if (SlotImage)
    {
        SlotImage->SetColorAndOpacity(bIsCooldown ? CooldownImageTint : FLinearColor::White);
    }
}