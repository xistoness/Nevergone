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
 *
 * Activation hooks come in pairs:
 *   - NativeOnPanelActivated / NativeOnPanelDeactivated  → C++ override point.
 *     Subclasses that need C++ logic when shown/hidden override these.
 *   - OnPanelActivated / OnPanelDeactivated              → BP-only event.
 *     Author-facing hook for animations, sounds, focus changes.
 *
 * The owning menu widget calls Activate()/Deactivate() (the wrappers below),
 * which dispatch both layers in order: native first, then Blueprint.
 */
UCLASS(Abstract)
class NEVERGONE_API UMenuPanelBase : public UUserWidget
{
	GENERATED_BODY()

public:

	/**
	 * Public dispatchers — called by the parent menu widget.
	 * Always invoke these instead of calling OnPanelActivated/Deactivated
	 * directly so the C++ override layer is not skipped.
	 */
	void Activate();
	void Deactivate();

	// Called by UMainMenuWidget when this panel becomes the active one (BP layer).
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu", meta = (DisplayName = "On Panel Activated"))
	void OnPanelActivated();

	// Called by UMainMenuWidget when this panel is hidden (BP layer).
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu", meta = (DisplayName = "On Panel Deactivated"))
	void OnPanelDeactivated();

	// Call from Blueprint when the panel's primary action should execute
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ConfirmPanel();

	// Delegates — subscribed by UMainMenuWidget
	FOnPanelConfirmed OnPanelConfirmed;
	FOnPanelNavigationRequested OnPanelNavigationRequested;

protected:

	/**
	 * C++ override point for activation. Default: no-op.
	 * Use this for state resets that should never be lost if a Blueprint
	 * subclass forgets to call Super in OnPanelActivated.
	 */
	virtual void NativeOnPanelActivated() {}

	/** C++ override point for deactivation. Default: no-op. */
	virtual void NativeOnPanelDeactivated() {}

	// Call from Blueprint to request navigation to another panel by ID
	// (e.g. "Quit" panel Cancel button navigating back to "Home")
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void RequestNavigation(FName PanelId);
};
