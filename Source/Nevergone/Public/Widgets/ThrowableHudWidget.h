// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ThrowableHudWidget.generated.h"

class UExcursionThrowComponent;
class UItemDefinition;
class UTexture2D;

// ---------------------------------------------------------------------------
// UThrowableHudWidget
//
// The small HUD icon of the throwable the next throw uses, with how many are
// left. Nothing else: the state of the zombies never shows on screen.
//
// Authority split:
//   - C++ owns visibility: collapsed while nothing can be thrown.
//   - BP   owns the layout (icon image, quantity text, animations).
//
// The BP MUST NOT call SetVisibility on the widget root.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UThrowableHudWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    /** Listens to the survivor's selection and shows it at once. Replaces any earlier binding. */
    void Bind(UExcursionThrowComponent* Throw);

    void Unbind();

protected:

    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    /** BP implements: show this icon and quantity. Already visible. */
    UFUNCTION(BlueprintImplementableEvent, Category = "ThrowableHud")
    void BP_SelectionChanged(UTexture2D* Icon, const FText& Name, int32 Quantity);

private:

    void HandleSelectionChanged(UItemDefinition* Item, int32 Quantity);

    TWeakObjectPtr<UExcursionThrowComponent> BoundThrow;

    FDelegateHandle SelectionHandle;
};
