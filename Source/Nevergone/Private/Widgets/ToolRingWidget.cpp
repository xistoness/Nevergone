// Copyright Xyzto Works

#include "Widgets/ToolRingWidget.h"

#include "Nevergone.h"
#include "ActorComponents/ExcursionThrowComponent.h"
#include "Data/ItemDefinition.h"

void UToolRingWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // Starts collapsed. The ring opening is the only thing that shows it.
    SetVisibility(ESlateVisibility::Collapsed);
}

void UToolRingWidget::NativeDestruct()
{
    Unbind();

    Super::NativeDestruct();
}

void UToolRingWidget::Bind(UExcursionThrowComponent* Throw)
{
    Unbind();

    if (!Throw)
    {
        return;
    }

    BoundThrow = Throw;

    OpenedHandle = Throw->OnRingOpened.AddUObject(this, &UToolRingWidget::HandleRingOpened);
    HighlightHandle = Throw->OnRingHighlightChanged.AddUObject(this, &UToolRingWidget::HandleHighlightChanged);
    ClosedHandle = Throw->OnRingClosed.AddUObject(this, &UToolRingWidget::HandleRingClosed);
}

void UToolRingWidget::Unbind()
{
    if (UExcursionThrowComponent* Throw = BoundThrow.Get())
    {
        Throw->OnRingOpened.Remove(OpenedHandle);
        Throw->OnRingHighlightChanged.Remove(HighlightHandle);
        Throw->OnRingClosed.Remove(ClosedHandle);
    }

    BoundThrow.Reset();

    SetVisibility(ESlateVisibility::Collapsed);
}

FVector2D UToolRingWidget::GetSliceDirection(int32 Index, int32 Count)
{
    if (Count <= 0)
    {
        return FVector2D::ZeroVector;
    }

    const float Angle = UE_TWO_PI * Index / Count;

    return FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle));
}

void UToolRingWidget::HandleRingOpened(const TArray<FThrowableOption>& Options, int32 Highlight)
{
    TArray<FToolRingSlice> Slices;
    Slices.Reserve(Options.Num());

    for (const FThrowableOption& Option : Options)
    {
        FToolRingSlice& Slice = Slices.AddDefaulted_GetRef();

        Slice.Icon = Option.Item ? Option.Item->Icon : nullptr;
        Slice.Name = Option.Item ? Option.Item->DisplayName : FText::GetEmpty();
        Slice.Quantity = Option.Quantity;
    }

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    BP_RingOpened(Slices, Highlight);
}

void UToolRingWidget::HandleHighlightChanged(int32 Highlight)
{
    BP_HighlightChanged(Highlight);
}

void UToolRingWidget::HandleRingClosed()
{
    BP_RingClosed();

    SetVisibility(ESlateVisibility::Collapsed);
}
