// Copyright Xyzto Works

#include "Widgets/Settings/VolumeSliderWidget.h"

#include "Audio/AudioSubsystem.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "VolumeSlider"

void UVolumeSliderWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Cache subsystem; lives for the entire session.
	if (UGameInstance* GI = GetGameInstance())
	{
		AudioSubsystem = GI->GetSubsystem<UAudioSubsystem>();
	}

	if (!AudioSubsystem)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[VolumeSliderWidget] NativeOnInitialized: AudioSubsystem unavailable"));
		return;
	}

	if (Btn_Prev)
	{
		Btn_Prev->OnClicked.AddDynamic(this, &UVolumeSliderWidget::HandlePrevClicked);
	}
	if (Btn_Next)
	{
		Btn_Next->OnClicked.AddDynamic(this, &UVolumeSliderWidget::HandleNextClicked);
	}

	// Apply the localizable header label (set per-instance in BP defaults).
	if (Lbl_VolumeLabel && !LabelText.IsEmpty())
	{
		Lbl_VolumeLabel->SetText(LabelText);
	}

	RefreshDisplay();

	UE_LOG(LogTemp, Log,
		TEXT("[VolumeSliderWidget] Initialized — Category=%d Step=%.2f Volume=%.2f"),
		static_cast<int32>(Category), VolumeStep, AudioSubsystem->GetCategoryVolume(Category));
}

// ---------------------------------------------------------------------------
// Button handlers
// ---------------------------------------------------------------------------

void UVolumeSliderWidget::HandlePrevClicked()
{
	AdjustVolume(-VolumeStep);
}

void UVolumeSliderWidget::HandleNextClicked()
{
	AdjustVolume(+VolumeStep);
}

void UVolumeSliderWidget::AdjustVolume(float Delta)
{
	if (!AudioSubsystem) { return; }

	const float Current = AudioSubsystem->GetCategoryVolume(Category);

	// SetCategoryVolume clamps to [0..1] internally — we still snap to the step
	// grid here so the displayed % values are predictable (10/20/30/...) instead
	// of drifting slightly from accumulated float math.
	const float Raw     = Current + Delta;
	const float Snapped = FMath::RoundToFloat(Raw / VolumeStep) * VolumeStep;
	const float NewVolume = FMath::Clamp(Snapped, 0.0f, 1.0f);

	if (FMath::IsNearlyEqual(NewVolume, Current))
	{
		// Already at the boundary; nothing to do (and avoids a redundant disk write).
		return;
	}

	AudioSubsystem->SetCategoryVolume(Category, NewVolume);
	RefreshDisplay();
}

void UVolumeSliderWidget::RefreshDisplay()
{
	if (!AudioSubsystem) { return; }

	const float Volume     = AudioSubsystem->GetCategoryVolume(Category);
	const int32 PercentInt = FMath::RoundToInt(Volume * 100.0f);

	if (Lbl_VolumeValue)
	{
		// FText::Format with AsNumber respects culture-specific number formatting
		// (e.g. some locales use a non-breaking space before "%"). Authors can
		// translate "{0}%" to a culture-appropriate pattern via the Localization Dashboard.
		Lbl_VolumeValue->SetText(FText::Format(
			LOCTEXT("VolumePercentFormat", "{0}%"),
			FText::AsNumber(PercentInt)));
	}

	if (ProgressBar_Volume)
	{
		ProgressBar_Volume->SetPercent(Volume);
	}
}

#undef LOCTEXT_NAMESPACE
