// Copyright Xyzto Works

#include "Widgets/Combat/FloatingCombatTextWidget.h"

#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"

void UFloatingCombatTextWidget::InitFloatingText(
    FVector           InWorldAnchor,
    const FString&    InText,
    EFloatingTextType InType,
    float             InScreenYOffset,
    UTexture2D*       InIcon)
{
    WorldAnchor   = InWorldAnchor;
    DisplayText   = InText;
    TextType      = InType;
    ScreenYOffset = InScreenYOffset;
    Icon          = InIcon;

    UE_LOG(LogTemp, Verbose,
        TEXT("[FloatingCombatTextWidget] InitFloatingText: text='%s' type=%d screenYOffset=%.1f"),
        *DisplayText, (int32)TextType, ScreenYOffset);

    // Populate the TextBlock immediately to avoid an empty frame
    if (FloatingText)
    {
        FloatingText->SetText(FText::FromString(DisplayText));
    }

    // Populate the icon if the slot exists in the UMG
    if (FloatingIcon)
    {
        if (Icon)
        {
            FloatingIcon->SetBrushFromTexture(Icon);
            FloatingIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            FloatingIcon->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Position the widget before the first frame to avoid a flash at the origin
    FVector2D InitialScreenPos;
    if (TryGetScreenPosition(InitialScreenPos))
    {
        SetPositionInViewport(InitialScreenPos, true);
        SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        // Anchor outside the frustum: hide but keep the widget alive
        // in case the camera returns before the animation ends
        SetVisibility(ESlateVisibility::Collapsed);
    }

    // Notify the Blueprint to trigger the intro animation
    OnFloatingTextReady();
}

void UFloatingCombatTextWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    FVector2D ScreenPos;
    if (TryGetScreenPosition(ScreenPos))
    {
        SetPositionInViewport(ScreenPos, true);

        // Show again if it was hidden for leaving the frustum
        if (GetVisibility() == ESlateVisibility::Collapsed)
        {
            SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }
    else
    {
        // Anchor outside the frustum or camera turned away: hide without destroying
        SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UFloatingCombatTextWidget::RequestDestroy()
{
    UE_LOG(LogTemp, Verbose,
        TEXT("[FloatingCombatTextWidget] RequestDestroy: text='%s'"), *DisplayText);

    OnDestroyed.ExecuteIfBound();
    RemoveFromParent();
}

bool UFloatingCombatTextWidget::TryGetScreenPosition(FVector2D& OutScreenPos) const
{
    const APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        return false;
    }

    const FVector AnchorWithOffset = WorldAnchor + FVector(0.f, 0.f, WorldZOffset);

    // bPlayerViewportRelative=false → absolute viewport coordinates,
    // required for SetPositionInViewport(bRemoveDPIScale=true)
    if (!PC->ProjectWorldLocationToScreen(AnchorWithOffset, OutScreenPos, false))
    {
        return false;
    }

    // Reject projections behind the camera.
    // ProjectWorldLocationToScreen may return true with valid X/Y even
    // when the point is behind — we check with a dot product against the camera direction.
    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    const FVector ToDot  = (AnchorWithOffset - CamLoc).GetSafeNormal();
    const FVector CamFwd = CamRot.Vector();
    if (FVector::DotProduct(ToDot, CamFwd) <= 0.f)
    {
        return false;
    }

    // Apply the per-widget screen-space stagger (negative Y = upward in screen space)
    OutScreenPos.Y -= ScreenYOffset;

    return true;
}