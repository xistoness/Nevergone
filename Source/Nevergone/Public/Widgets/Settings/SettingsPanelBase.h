// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Menu/MenuPanelBase.h"
#include "SettingsPanelBase.generated.h"

class UWidgetSwitcher;
class USettingsTabPanelBase;

/**
 * USettingsPanelBase
 *
 * Settings panel inside the Main Menu and Pause Menu.
 *
 * Holds a UWidgetSwitcher with one USettingsTabPanelBase per tab
 * (Game, Audio, ...). Owns the tab navigation state and exposes a small
 * API for the parent menu / Blueprint sidebar to drive tab changes.
 *
 * Author contract — WBP_Panel_Settings must:
 *   - inherit USettingsPanelBase
 *   - contain a UWidgetSwitcher named exactly "TabSwitcher"
 *   - contain one BindWidget child per declared tab (Tab_Game, Tab_Audio, ...)
 *
 * Adding a new tab later (e.g. Video, Controls):
 *   1. Create UClass USettingsTabPanel_Video : USettingsTabPanelBase
 *   2. Add WBP_Tab_Video as child of it
 *   3. Add BindWidget UPROPERTY here (Tab_Video)
 *   4. Append "Video" to TabOrder in NativeOnInitialized
 *   5. Register in TabMap inside RegisterTabs()
 */
UCLASS(Abstract)
class NEVERGONE_API USettingsPanelBase : public UMenuPanelBase
{
	GENERATED_BODY()

public:

	virtual void NativeOnInitialized() override;

	// -----------------------------------------------------------------------
	// Tab API (callable from BP sidebar buttons or input handlers)
	// -----------------------------------------------------------------------

	/** Switches the visible tab. No-op if TabId is unknown. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Tabs")
	void NavigateToTab(FName TabId);

	/** Cycles to the previous tab in TabOrder, wrapping around. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Tabs")
	void NavigateTabPrev();

	/** Cycles to the next tab in TabOrder, wrapping around. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Tabs")
	void NavigateTabNext();

	/** Returns the ID of the currently active tab, or NAME_None if none active. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Settings|Tabs")
	FName GetActiveTabId() const { return ActiveTabId; }

protected:

	// -----------------------------------------------------------------------
	// MenuPanelBase overrides
	// -----------------------------------------------------------------------

	/**
	 * Resets to the first tab whenever the Settings panel is shown. Matches
	 * the UX of the parent menu (PauseMenu always opens on Resume; we always
	 * open Settings on the first tab).
	 */
	virtual void NativeOnPanelActivated() override;

	// -----------------------------------------------------------------------
	// BindWidget — names must match exactly in WBP_Panel_Settings
	// -----------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UWidgetSwitcher* TabSwitcher;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	USettingsTabPanelBase* Tab_Game;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	USettingsTabPanelBase* Tab_Audio;

	// -----------------------------------------------------------------------
	// BP visual hooks — for the sidebar / tab strip artwork
	// -----------------------------------------------------------------------

	/**
	 * Called whenever the active tab index changes. Implement in BP to
	 * update the visual state of the tab strip (highlight, underline, etc.).
	 *
	 * @param NewIndex     Index of the now-active tab in TabOrder
	 * @param NewTabId     Identifier of the now-active tab
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings|Tabs")
	void BP_OnActiveTabChanged(int32 NewIndex, FName NewTabId);

private:

	/** Builds TabMap from the BindWidget tabs. Called once from NativeOnInitialized. */
	void RegisterTabs();

	/** Display order of tabs in the strip. Used by NavigateTabPrev/Next. */
	UPROPERTY()
	TArray<FName> TabOrder;

	/** Resolves a TabId to its widget. Built from BindWidget pointers in RegisterTabs. */
	UPROPERTY()
	TMap<FName, USettingsTabPanelBase*> TabMap;

	/** ID of the currently visible tab. NAME_None until NativeOnInitialized runs. */
	FName ActiveTabId = NAME_None;
};
