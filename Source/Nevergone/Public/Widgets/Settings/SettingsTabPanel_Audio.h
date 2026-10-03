// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Settings/SettingsTabPanelBase.h"
#include "SettingsTabPanel_Audio.generated.h"

class UVolumeSliderWidget;

/**
 * USettingsTabPanel_Audio
 *
 * "Audio" tab inside the Settings panel. Hosts one UVolumeSliderWidget per
 * mixing category. Each slider is fully self-contained — this tab only needs
 * BindWidget references so the layout exists and (later) focus handoff can
 * walk through them in order.
 *
 * Author contract — WBP_Tab_Settings_Audio must:
 *   - inherit USettingsTabPanel_Audio
 *   - contain four UVolumeSliderWidget instances named:
 *       "VolumeSlider_Master" / "VolumeSlider_Music" /
 *       "VolumeSlider_SFX"    / "VolumeSlider_UI"
 *   - On each instance, set Class Defaults:
 *       Category   → matching ENevergoneAudioCategory
 *       LabelText  → localizable header (e.g. LOCTEXT key "Master Volume")
 */
UCLASS(Abstract)
class NEVERGONE_API USettingsTabPanel_Audio : public USettingsTabPanelBase
{
	GENERATED_BODY()

protected:

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVolumeSliderWidget* VolumeSlider_Master;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVolumeSliderWidget* VolumeSlider_Music;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVolumeSliderWidget* VolumeSlider_SFX;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVolumeSliderWidget* VolumeSlider_UI;
};
