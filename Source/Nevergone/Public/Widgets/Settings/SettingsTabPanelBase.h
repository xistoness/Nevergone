// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Menu/MenuPanelBase.h"
#include "SettingsTabPanelBase.generated.h"

/**
 * USettingsTabPanelBase
 *
 * Base class for individual tabs inside the Settings panel
 * (Game, Audio, Video, Controls, ...).
 *
 * Inherits from UMenuPanelBase to reuse the activation lifecycle
 * (NativeOnPanelActivated / NativeOnPanelDeactivated, BP OnPanelActivated /
 * OnPanelDeactivated). Tabs do NOT use ConfirmPanel — Settings has no
 * "Apply" workflow because each control commits its change immediately
 * (volumes write to disk per slider tick, language switches the live mix
 * on the spot, etc.).
 *
 * Concrete tabs (USettingsTabPanel_Game, USettingsTabPanel_Audio) inherit
 * from this and BindWidget their internal controls.
 */
UCLASS(Abstract)
class NEVERGONE_API USettingsTabPanelBase : public UMenuPanelBase
{
	GENERATED_BODY()
};
