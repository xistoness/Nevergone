// Copyright Xyzto Works

#include "Widgets/Menu/MainMenuWidget.h"

#include "Audio/AudioSubsystem.h"
#include "Components/WidgetSwitcher.h"
#include "GameInstance/MyGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Menu/MenuPanelBase.h"

class UAudioSubsystem;

void UMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Register each panel with its confirm handler.
	// NavigateTo() uses NavigationMap, so order here does not matter.
	BindPanel("NewGame",  Panel_NewGame,  [this]() { HandleNewGameConfirmed();  });
	BindPanel("Continue", Panel_Continue, [this]() { HandleContinueConfirmed(); });
	BindPanel("Load",     Panel_Load,     [this]() { HandleLoadConfirmed();     });
	BindPanel("Settings", Panel_Settings, [this]() { HandleSettingsConfirmed(); });
	BindPanel("Quit",     Panel_Quit,     [this]() { HandleQuitConfirmed();     });

	// Open the default panel on startup
	NavigateTo("NewGame");
}

void UMainMenuWidget::NavigateTo(FName PanelId)
{
	UMenuPanelBase** Found = NavigationMap.Find(PanelId);
	if (!Found || !*Found)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainMenuWidget] NavigateTo: unknown panel '%s'"), *PanelId.ToString());
		return;
	}

	// Deactivate the current panel
	if (!ActivePanelId.IsNone())
	{
		if (UMenuPanelBase** Current = NavigationMap.Find(ActivePanelId))
		{
			(*Current)->OnPanelDeactivated();
		}
	}

	// Switch the visible widget
	PanelSwitcher->SetActiveWidget(*Found);
	ActivePanelId = PanelId;

	// Notify the new panel
	(*Found)->OnPanelActivated();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
		{
			Audio->PlayUISoundEvent(EUISoundEvent::TabSwitch);
		}
	}
}

// --- Private helpers ---

void UMainMenuWidget::BindPanel(FName PanelId, UMenuPanelBase* Panel, TFunction<void()> OnConfirm)
{
	if (!Panel)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainMenuWidget] BindPanel: panel '%s' is null — check BindWidget names in WBP_MainMenu"), *PanelId.ToString());
		return;
	}

	NavigationMap.Add(PanelId, Panel);

	// Wire the confirm delegate
	Panel->OnPanelConfirmed.AddLambda(OnConfirm);

	// Wire navigation requests so panels can redirect (e.g. Quit -> Cancel -> NewGame)
	Panel->OnPanelNavigationRequested.AddLambda([this](FName TargetId)
	{
		NavigateTo(TargetId);
	});
}

UMyGameInstance* UMainMenuWidget::GetGI() const
{
	return GetWorld() ? GetWorld()->GetGameInstance<UMyGameInstance>() : nullptr;
}

// --- Confirm handlers ---

void UMainMenuWidget::HandleNewGameConfirmed()
{
	UMyGameInstance* GI = GetGI();
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("[MainMenuWidget] HandleNewGameConfirmed: GameInstance invalid"));
		return;
	}

	// Wipes the existing MainSlot and creates a fresh one
	GI->RequestNewGame();

	if (FirstLevelName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainMenuWidget] HandleNewGameConfirmed: FirstLevelName not set — set it in WBP_MainMenu defaults"));
		return;
	}
	
	if (GI)
	{
		if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
		{
			Audio->PlayUISoundEvent(EUISoundEvent::ButtonConfirm);
		}
	}

	FLevelTransitionContext Context;
	Context.FromLevel = NAME_None;
	Context.ToLevel   = FirstLevelName;
	GI->RequestLevelChange(FirstLevelName, Context);
}

void UMainMenuWidget::HandleContinueConfirmed()
{
	UMyGameInstance* GI = GetGI();
	if (!GI) return;

	// Save is already loaded by the panel before ConfirmPanel was called.
	// Just transition to the saved level.
	const FName SavedLevel = GI->GetActiveSavedLevelName();
	if (SavedLevel.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainMenuWidget] HandleContinueConfirmed: no saved level — going to first level"));
		if (!FirstLevelName.IsNone())
		{
			FLevelTransitionContext Context;
			Context.ToLevel = FirstLevelName;
			GI->RequestLevelChange(FirstLevelName, Context);
		}
		return;
	}
	
	if (GI)
	{
		if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
		{
			Audio->PlayUISoundEvent(EUISoundEvent::ButtonConfirm);
		}
	}

	FLevelTransitionContext Context;
	Context.ToLevel = SavedLevel;
	GI->RequestLevelChange(SavedLevel, Context);
}

void UMainMenuWidget::HandleLoadConfirmed()
{
	HandleContinueConfirmed();
}

void UMainMenuWidget::HandleSettingsConfirmed()
{
	// Settings panel applies its own changes internally (volume, resolution, etc.)
	// and calls ConfirmPanel() when the player clicks "Apply & Save".
	// The GI save for settings lives separately from the gameplay save —
	// for now this is a no-op here; the panel handles it via BlueprintCallable.
	UE_LOG(LogTemp, Log, TEXT("[MainMenuWidget] Settings confirmed (applied by panel)"));
}

void UMainMenuWidget::HandleQuitConfirmed()
{
	UMyGameInstance* GI = GetGI();
	if (GI)
	{
		// Autosave before quitting
		GI->RequestSaveGame();
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}
	
	if (GI)
	{
		if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
		{
			Audio->PlayUISoundEvent(EUISoundEvent::ButtonConfirm);
		}
	}

	UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
}