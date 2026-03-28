// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UWidgetSwitcher;
class UMenuPanelBase;
class UMyGameInstance;

/**
 * Root main menu widget.
 *
 * Owns navigation between panels and bridges panel actions to MyGameInstance.
 * The Blueprint child (WBP_MainMenu) must contain a UWidgetSwitcher named
 * "PanelSwitcher" and one widget per panel — each named exactly as declared
 * below (BindWidget enforces this at compile time).
 *
 * To add a new panel:
 *   1. Create WBP_Panel_X as a child of UMenuPanelBase.
 *   2. Add a UPROPERTY BindWidget here for it.
 *   3. Call BindPanel() in NativeOnInitialized with the desired action.
 *   4. Add an entry to the NavigationMap in NativeOnInitialized.
 */
UCLASS()
class NEVERGONE_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	virtual void NativeOnInitialized() override;

	// Navigate to a panel by its registered ID.
	// IDs match the FName keys in NavigationMap ("NewGame", "Continue", etc.)
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void NavigateTo(FName PanelId);

protected:

	// --- BindWidget: names must match exactly in WBP_MainMenu ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UWidgetSwitcher* PanelSwitcher;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UMenuPanelBase* Panel_NewGame;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UMenuPanelBase* Panel_Continue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UMenuPanelBase* Panel_Load;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UMenuPanelBase* Panel_Settings;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UMenuPanelBase* Panel_Quit;

	// --- First level to open after starting or continuing ---

	UPROPERTY(EditDefaultsOnly, Category = "Menu")
	FName FirstLevelName = NAME_None;

private:

	// Registers a panel into the navigation map and wires its OnConfirmed delegate
	void BindPanel(FName PanelId, UMenuPanelBase* Panel, TFunction<void()> OnConfirm);

	// Maps panel IDs to their widget instances — used by NavigateTo
	TMap<FName, UMenuPanelBase*> NavigationMap;

	// Tracks the currently active panel
	FName ActivePanelId;

	UMyGameInstance* GetGI() const;

	// --- Confirm handlers, one per panel ---

	void HandleNewGameConfirmed();
	void HandleContinueConfirmed();
	void HandleLoadConfirmed();
	void HandleSettingsConfirmed();
	void HandleQuitConfirmed();
};