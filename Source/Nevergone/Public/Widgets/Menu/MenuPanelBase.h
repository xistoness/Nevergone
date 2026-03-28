// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MenuPanelBase.generated.h"

// Fired when the panel's primary action is confirmed (e.g. "Start", "Load", "Apply")
DECLARE_MULTICAST_DELEGATE(FOnPanelConfirmed);

// Fired when the panel requests navigation to another panel
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPanelNavigationRequested, FName /*PanelId*/);

/**
 * Base class for all main menu panels.
 *
 * Each panel (New Game, Continue, Load, Settings, Quit) inherits from this.
 * The panel owns its visual logic in Blueprint/UMG; the main menu widget
 * subscribes to the delegates below to react without coupling to panel internals.
 */
UCLASS(Abstract)
class NEVERGONE_API UMenuPanelBase : public UUserWidget
{
	GENERATED_BODY()

public:

	// Called by UMainMenuWidget when this panel becomes the active one
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void OnPanelActivated();

	// Called by UMainMenuWidget when this panel is hidden
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void OnPanelDeactivated();

	// Delegates — subscribed by UMainMenuWidget
	FOnPanelConfirmed OnPanelConfirmed;
	FOnPanelNavigationRequested OnPanelNavigationRequested;

protected:

	// Call from Blueprint when the panel's primary action should execute
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ConfirmPanel();

	// Call from Blueprint to request navigation to another panel by ID
	// (e.g. "Quit" panel Cancel button navigating back to "Home")
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void RequestNavigation(FName PanelId);
};