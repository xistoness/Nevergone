// Copyright Xyzto Works

#include "Widgets/ThrowableHudWidget.h"

#include "Nevergone.h"
#include "ActorComponents/ExcursionThrowComponent.h"
#include "Data/ItemDefinition.h"

void UThrowableHudWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    SetVisibility(ESlateVisibility::Collapsed);
}

void UThrowableHudWidget::NativeDestruct()
{
    Unbind();

    Super::NativeDestruct();
}

void UThrowableHudWidget::Bind(UExcursionThrowComponent* Throw)
{
    Unbind();

    if (!Throw)
    {
        return;
    }

    BoundThrow = Throw;

    SelectionHandle = Throw->OnSelectionChanged.AddUObject(this, &UThrowableHudWidget::HandleSelectionChanged);

    Throw->RefreshSelection();

    HandleSelectionChanged(Throw->GetSelectedThrowable(), Throw->GetSelectedQuantity());
}

void UThrowableHudWidget::Unbind()
{
    if (UExcursionThrowComponent* Throw = BoundThrow.Get())
    {
        Throw->OnSelectionChanged.Remove(SelectionHandle);
    }

    BoundThrow.Reset();

    SetVisibility(ESlateVisibility::Collapsed);
}

void UThrowableHudWidget::HandleSelectionChanged(UItemDefinition* Item, int32 Quantity)
{
    if (!Item)
    {
        SetVisibility(ESlateVisibility::Collapsed);

        return;
    }

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    BP_SelectionChanged(Item->Icon, Item->DisplayName, Quantity);
}
