// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Settings/SettingsTabPanelBase.h"
#include "SettingsTabPanel_Game.generated.h"

class ULanguageSelectorWidget;

/**
 * USettingsTabPanel_Game
 *
 * "Game" tab inside the Settings panel. Currently houses the language
 * selector — additional gameplay-specific options (difficulty, autosave
 * frequency, tutorial toggles) will be added here as new BindWidget controls.
 *
 * Author contract — WBP_Tab_Settings_Game must:
 *   - inherit USettingsTabPanel_Game
 *   - contain a ULanguageSelectorWidget named "LanguageSelector"
 */
UCLASS(Abstract)
class NEVERGONE_API USettingsTabPanel_Game : public USettingsTabPanelBase
{
	GENERATED_BODY()

protected:

	/**
	 * Reference to the language selector control. The control is fully
	 * self-contained (it owns its own state and subsystem subscription),
	 * so the tab does not need to drive it — this BindWidget exists so
	 * future logic (e.g. focus handoff for joystick navigation) can find it.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	ULanguageSelectorWidget* LanguageSelector;
};
