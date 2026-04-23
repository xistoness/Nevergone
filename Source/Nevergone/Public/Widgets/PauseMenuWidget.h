// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UWidgetSwitcher;
class UMenuPanelBase;
class UMyGameInstance;

DECLARE_MULTICAST_DELEGATE(FOnResumeRequested);

/**
 * UPauseMenuWidget
 *
 * In-game pause menu. Follows the same PanelSwitcher + MenuPanelBase pattern
 * as UMainMenuWidget. Panels: Resume, Save, Load, Settings, Quit.
 *
 * Navigation model (mirrors QuestLogWidget):
 *  - NavigateUp / NavigateDown move the cursor through the sidebar entries.
 *  - ConfirmSelection executes the primary action of the active panel.
 *  - CancelSelection navigates back to Resume (or closes if already there).
 *
 * BP implementation contract (WBP_PauseMenu):
 *  - Must contain a UWidgetSwitcher named "PanelSwitcher".
 *  - Must contain UMenuPanelBase widgets with the exact names below (BindWidget).
 *  - BP_SetSidebarEntrySelected(int32 Index, bool bSelected): highlight the
 *    sidebar button at Index. Called by C++ on cursor movement.
 */
UCLASS()
class NEVERGONE_API UPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    virtual void NativeOnInitialized() override;

    // Called by PauseMenuComponent when the menu opens — resets to Resume panel.
    UFUNCTION(BlueprintCallable, Category = "Pause")
    void RefreshOnOpen();

    // Navigate to a panel by ID ("Resume", "Save", "Load", "Settings", "Quit").
    UFUNCTION(BlueprintCallable, Category = "Pause")
    void NavigateTo(FName PanelId);

    // --- Keyboard navigation (called by ExplorationPlayerController) ---

    UFUNCTION(BlueprintCallable, Category = "Pause")
    void NavigateUp();

    UFUNCTION(BlueprintCallable, Category = "Pause")
    void NavigateDown();

    // Executes the primary action of the current panel (calls ConfirmPanel).
    UFUNCTION(BlueprintCallable, Category = "Pause")
    void ConfirmSelection();

    // Navigates back to Resume if not already there; returns false if already
    // on Resume (controller should close the menu).
    UFUNCTION(BlueprintCallable, Category = "Pause")
    bool CancelSelection();

    // Fired when Resume is confirmed — PauseMenuComponent binds to this.
    FOnResumeRequested OnResumeRequested;

protected:

    // --- BindWidget: names must match exactly in WBP_PauseMenu ---

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UWidgetSwitcher* PanelSwitcher;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UMenuPanelBase* Panel_Resume;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UMenuPanelBase* Panel_Save;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UMenuPanelBase* Panel_Load;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UMenuPanelBase* Panel_Settings;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UMenuPanelBase* Panel_Quit;
    
    // Index of the slot selected by the player in the Save panel.
    // Set by Blueprint when the player clicks a slot entry.
    // INDEX_NONE means no slot selected — save will use FindNextAvailableSlot.
    UPROPERTY(BlueprintReadWrite, Category = "Pause")
    int32 SelectedSlotIndex = INDEX_NONE;
    
    UFUNCTION(BlueprintCallable, Category = "Pause")
    void SetSelectedSlotIndex(int32 InSlotIndex) { SelectedSlotIndex = InSlotIndex; }

    UFUNCTION(BlueprintPure, Category = "Pause")
    int32 GetSelectedSlotIndex() const { return SelectedSlotIndex; }

    // Called on cursor movement — highlight the sidebar button at Index.
    UFUNCTION(BlueprintImplementableEvent, Category = "Pause")
    void BP_SetSidebarEntrySelected(int32 Index, bool bSelected);

private:

    void BindPanel(FName PanelId, UMenuPanelBase* Panel, TFunction<void()> OnConfirm);
    void SetCursorIndex(int32 NewIndex);

    // Ordered list matching sidebar button order — cursor indexes into this.
    TArray<FName> PanelOrder;

    TMap<FName, UMenuPanelBase*> NavigationMap;
    FName ActivePanelId;
    int32 CursorIndex = 0;

    UMyGameInstance* GetGI() const;

    void HandleResumeConfirmed();
    void HandleSaveConfirmed();
    void HandleLoadConfirmed();
    void HandleSettingsConfirmed();
    void HandleQuitConfirmed();
};