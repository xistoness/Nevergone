// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ToolRingWidget.generated.h"

class UExcursionThrowComponent;
class UTexture2D;
struct FThrowableOption;

// One slice of the ring, as the layout needs it.
USTRUCT(BlueprintType)
struct FToolRingSlice
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "ToolRing")
    TObjectPtr<UTexture2D> Icon = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "ToolRing")
    FText Name;

    UPROPERTY(BlueprintReadOnly, Category = "ToolRing")
    int32 Quantity = 0;
};

// ---------------------------------------------------------------------------
// UToolRingWidget
//
// The ring that picks what the survivor throws. It only shows what
// UExcursionThrowComponent decides: the component owns the pointer, the
// highlight and the choice, and this widget listens to it.
//
// Authority split:
//   - C++ owns visibility (Collapsed <-> SelfHitTestInvisible), so the ring
//     never takes a click and never stays on screen after it closes.
//   - BP   owns the layout: one slice per BP_RingOpened entry placed around
//     the centre (GetSliceDirection gives where), the highlight look, and
//     any animation.
//
// The BP MUST NOT call SetVisibility on the widget root.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UToolRingWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    /** Listens to the survivor's ring. Replaces any earlier binding. */
    void Bind(UExcursionThrowComponent* Throw);

    void Unbind();

    /**
     * Screen-space unit direction (X right, Y down) of a slice from the
     * centre: slice 0 on top, the rest clockwise. Matches the highlight the
     * component computes from the pointer.
     */
    UFUNCTION(BlueprintPure, Category = "ToolRing")
    static FVector2D GetSliceDirection(int32 Index, int32 Count);

protected:

    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    /** BP implements: build the slices and show the highlight. Already visible. */
    UFUNCTION(BlueprintImplementableEvent, Category = "ToolRing")
    void BP_RingOpened(const TArray<FToolRingSlice>& Slices, int32 Highlight);

    /** BP implements: move the highlight to this slice. */
    UFUNCTION(BlueprintImplementableEvent, Category = "ToolRing")
    void BP_HighlightChanged(int32 Highlight);

    /** BP implements: clear the slices. The widget collapses right after. */
    UFUNCTION(BlueprintImplementableEvent, Category = "ToolRing")
    void BP_RingClosed();

private:

    void HandleRingOpened(const TArray<FThrowableOption>& Options, int32 Highlight);
    void HandleHighlightChanged(int32 Highlight);
    void HandleRingClosed();

    TWeakObjectPtr<UExcursionThrowComponent> BoundThrow;

    FDelegateHandle OpenedHandle;
    FDelegateHandle HighlightHandle;
    FDelegateHandle ClosedHandle;
};
