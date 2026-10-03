// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Audio/AudioCategory.h"
#include "Blueprint/UserWidget.h"
#include "VolumeSliderWidget.generated.h"

class UButton;
class UTextBlock;
class UProgressBar;
class UAudioSubsystem;

/**
 * UVolumeSliderWidget
 *
 * Joystick-friendly per-category volume control. Layout:
 *
 *     <CategoryLabel>     ◀  [████████░░]  80%  ▶
 *
 * Reusable for every category — each WBP_VolumeSlider_* instance just sets
 * `Category` and `LabelText` in the Class Defaults of its containing tab.
 *
 * Author contract — WBP_VolumeSlider must:
 *   - inherit UVolumeSliderWidget
 *   - contain a UButton named "Btn_Prev"
 *   - contain a UButton named "Btn_Next"
 *   - contain a UTextBlock named "Lbl_VolumeLabel"  (header)
 *   - contain a UTextBlock named "Lbl_VolumeValue"  (the percentage)
 *   - optionally a UProgressBar named "ProgressBar_Volume" (visual fill)
 */
UCLASS(Abstract)
class NEVERGONE_API UVolumeSliderWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	virtual void NativeOnInitialized() override;

	// -----------------------------------------------------------------------
	// Editor-facing config
	// -----------------------------------------------------------------------

	/** Which audio category this slider controls. Set per-instance in the parent UMG. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volume Slider")
	ENevergoneAudioCategory Category = ENevergoneAudioCategory::Master;

	/** Localizable header text (e.g. "Master Volume"). Set per-instance in the parent UMG. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volume Slider")
	FText LabelText;

	/**
	 * Volume increment per click of ◀ / ▶, in normalized [0..1].
	 * 0.05 = 5% steps; 0.10 = 10% steps. Default is 0.10 so the player
	 * reaches min/max in 10 clicks — comfortable for joystick navigation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volume Slider", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float VolumeStep = 0.10f;

protected:

	// -----------------------------------------------------------------------
	// BindWidget — names must match exactly in WBP_VolumeSlider
	// -----------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Btn_Prev;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Btn_Next;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* Lbl_VolumeLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* Lbl_VolumeValue;

	/** Optional — leave unbound if the WBP only uses the percentage label. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UProgressBar* ProgressBar_Volume;

private:

	// -----------------------------------------------------------------------
	// Button handlers
	// -----------------------------------------------------------------------

	UFUNCTION()
	void HandlePrevClicked();

	UFUNCTION()
	void HandleNextClicked();

	/** Adjusts the persisted+live volume by the given delta and refreshes the label. */
	void AdjustVolume(float Delta);

	/** Re-reads the persisted volume from settings and updates the visual elements. */
	void RefreshDisplay();

	/** Cached subsystem reference. */
	UPROPERTY()
	TObjectPtr<UAudioSubsystem> AudioSubsystem;
};
