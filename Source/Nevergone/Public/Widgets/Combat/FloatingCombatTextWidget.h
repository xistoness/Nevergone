// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FloatingCombatTextWidget.generated.h"

class UTextBlock;
class UImage;

UENUM(BlueprintType)
enum class EFloatingTextType : uint8
{
    Damage       UMETA(DisplayName = "Damage"),
    Heal         UMETA(DisplayName = "Heal"),
    StatusApply  UMETA(DisplayName = "Status Apply"),
    StatusClear  UMETA(DisplayName = "Status Clear"),
    Miss         UMETA(DisplayName = "Miss"),
    ActionPoints UMETA(DisplayName = "Action Points"),
};

DECLARE_DELEGATE(FOnFloatingTextDestroyed);

/**
 * Agnostic floating combat text widget.
 *
 * C++ responsibilities:
 *   - Store the WorldAnchor (3D position the widget follows)
 *   - Reproject world → screen every tick
 *   - Hide the widget if the position leaves the frustum
 *   - Apply a per-widget screen-space Y stagger to avoid overlap
 *   - Expose data (text, type, icon) for the Blueprint to animate
 *
 * Blueprint responsibilities:
 *   - Float-up, fade-out, and scale pop animations (via UMG Animations)
 *   - Call RequestDestroy() at the end of the animation
 *   - Choose color and icon based on GetTextType()
 */
UCLASS(Abstract)
class NEVERGONE_API UFloatingCombatTextWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    /**
     * Initializes the widget with all required data.
     * Must be called immediately after CreateWidget, before AddToViewport.
     *
     * @param InWorldAnchor      3D position (world space) that the widget will follow
     * @param InText             Text to display (e.g., "42", "STUN", "MISS")
     * @param InType             Semantic type — controls color and icon in the Blueprint
     * @param InScreenYOffset    Additional upward offset in screen pixels to avoid overlap
     *                           with other widgets spawned at the same anchor this frame
     * @param InIcon             Optional icon; nullptr = no icon (text only)
     */
    UFUNCTION(BlueprintCallable, Category = "Floating Text")
    void InitFloatingText(
        FVector             InWorldAnchor,
        const FString&      InText,
        EFloatingTextType   InType,
        float               InScreenYOffset = 0.f,
        UTexture2D*         InIcon = nullptr
    );

    /** Called by the Blueprint on the last frame of the animation to destroy the widget. */
    UFUNCTION(BlueprintCallable, Category = "Floating Text")
    void RequestDestroy();

    // --- Getters for Blueprint ---

    UFUNCTION(BlueprintPure, Category = "Floating Text")
    EFloatingTextType GetTextType() const { return TextType; }

    UFUNCTION(BlueprintPure, Category = "Floating Text")
    UTexture2D* GetIcon() const { return Icon; }

    UFUNCTION(BlueprintPure, Category = "Floating Text")
    FString GetDisplayText() const { return DisplayText; }

    /**
     * Fired when RequestDestroy() is called so the owning character
     * can decrement its active widget counter.
     */
    FOnFloatingTextDestroyed OnDestroyed;

protected:

    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /**
     * Called by the Blueprint to trigger the intro animation.
     * Implement in the Blueprint to call PlayAnimation(IntroAnim).
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Floating Text")
    void OnFloatingTextReady();

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> FloatingText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> FloatingIcon;

private:

    /** World-space anchor that the widget follows frame by frame. */
    FVector WorldAnchor = FVector::ZeroVector;

    /** Vertical offset in world units applied to the anchor (raises the text above the head). */
    UPROPERTY(EditDefaultsOnly, Category = "Floating Text|Layout", meta = (ClampMin = "0"))
    float WorldZOffset = 20.f;

    /**
     * Additional upward offset in screen pixels applied on top of the projected position.
     * Set per-widget by the spawner to stagger simultaneous floating texts vertically.
     */
    float ScreenYOffset = 0.f;

    FString           DisplayText;
    EFloatingTextType TextType     = EFloatingTextType::Damage;
    TObjectPtr<UTexture2D> Icon    = nullptr;

    /** Reprojects WorldAnchor to screen space. Returns false if it is outside the frustum. */
    bool TryGetScreenPosition(FVector2D& OutScreenPos) const;
};