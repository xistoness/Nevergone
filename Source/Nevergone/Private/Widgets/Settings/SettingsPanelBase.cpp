// Copyright Xyzto Works

#include "Widgets/Settings/SettingsPanelBase.h"

#include "Audio/AudioSubsystem.h"
#include "Components/WidgetSwitcher.h"
#include "Widgets/Settings/SettingsTabPanelBase.h"

void USettingsPanelBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Display order of tabs in the strip. Editing this list also changes
	// the order of NavigateTabPrev/Next cycling.
	TabOrder = { TEXT("Game"), TEXT("Audio") };

	RegisterTabs();

	// Open on the first tab. NativeOnPanelActivated will re-trigger this every
	// time the panel becomes visible — that path covers reopens, this one
	// covers the very first show before any activation event.
	if (TabOrder.Num() > 0)
	{
		NavigateToTab(TabOrder[0]);
	}
}

void USettingsPanelBase::NativeOnPanelActivated()
{
	Super::NativeOnPanelActivated();

	// Always reset to the first tab on (re)open. Mirrors PauseMenu's
	// "always open on Resume" UX so the player has a predictable starting point.
	if (TabOrder.Num() > 0)
	{
		NavigateToTab(TabOrder[0]);
	}
}

// ---------------------------------------------------------------------------
// Tab API
// ---------------------------------------------------------------------------

void USettingsPanelBase::NavigateToTab(FName TabId)
{
	USettingsTabPanelBase** Found = TabMap.Find(TabId);
	if (!Found || !*Found)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SettingsPanelBase] NavigateToTab: unknown tab '%s' — check TabOrder and BindWidget names"),
			*TabId.ToString());
		return;
	}

	if (!TabSwitcher)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[SettingsPanelBase] NavigateToTab: TabSwitcher is null — check BindWidget name in WBP_Panel_Settings"));
		return;
	}

	// Deactivate the previous tab before swapping the widget so its
	// NativeOnPanelDeactivated runs before the next tab's activation.
	if (!ActiveTabId.IsNone() && ActiveTabId != TabId)
	{
		if (USettingsTabPanelBase** PreviousTab = TabMap.Find(ActiveTabId))
		{
			(*PreviousTab)->Deactivate();
		}
	}

	TabSwitcher->SetActiveWidget(*Found);
	ActiveTabId = TabId;

	(*Found)->Activate();

	const int32 NewIndex = TabOrder.IndexOfByKey(TabId);
	BP_OnActiveTabChanged(NewIndex, TabId);

	// UI feedback consistent with the rest of the menu chrome.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
		{
			Audio->PlayUISoundEvent(EUISoundEvent::TabSwitch);
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("[SettingsPanelBase] Active tab -> '%s' (index %d)"),
		*TabId.ToString(), NewIndex);
}

void USettingsPanelBase::NavigateTabPrev()
{
	if (TabOrder.IsEmpty()) { return; }

	const int32 CurrentIndex = TabOrder.IndexOfByKey(ActiveTabId);
	const int32 PrevIndex    = (CurrentIndex <= 0)
		? TabOrder.Num() - 1                    // wrap to last when at first or unknown
		: CurrentIndex - 1;

	NavigateToTab(TabOrder[PrevIndex]);
}

void USettingsPanelBase::NavigateTabNext()
{
	if (TabOrder.IsEmpty()) { return; }

	const int32 CurrentIndex = TabOrder.IndexOfByKey(ActiveTabId);
	const int32 NextIndex    = (CurrentIndex == INDEX_NONE || CurrentIndex == TabOrder.Num() - 1)
		? 0                                     // wrap to first when at last or unknown
		: CurrentIndex + 1;

	NavigateToTab(TabOrder[NextIndex]);
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void USettingsPanelBase::RegisterTabs()
{
	// Match each TabId in TabOrder to its BindWidget pointer.
	// If a tab is added to TabOrder but no BindWidget pointer exists for it,
	// the TabMap entry will be skipped and NavigateToTab will warn at runtime.

	if (Tab_Game)
	{
		TabMap.Add(TEXT("Game"), Tab_Game);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SettingsPanelBase] RegisterTabs: Tab_Game is null — check BindWidget in WBP_Panel_Settings"));
	}

	if (Tab_Audio)
	{
		TabMap.Add(TEXT("Audio"), Tab_Audio);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SettingsPanelBase] RegisterTabs: Tab_Audio is null — check BindWidget in WBP_Panel_Settings"));
	}
}
