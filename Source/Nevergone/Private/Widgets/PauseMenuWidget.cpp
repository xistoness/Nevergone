// Copyright Xyzto Works

#include "Widgets/PauseMenuWidget.h"

#include "Audio/AudioSubsystem.h"
#include "Components/WidgetSwitcher.h"
#include "GameInstance/MyGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Menu/MenuPanelBase.h"

void UPauseMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // Sidebar order must match the visual order in WBP_PauseMenu.
    PanelOrder = { "Resume", "Save", "Load", "Settings", "Quit" };

    BindPanel("Resume",   Panel_Resume,   [this]() { HandleResumeConfirmed();   });
    BindPanel("Save",     Panel_Save,     [this]() { HandleSaveConfirmed();     });
    BindPanel("Load",     Panel_Load,     [this]() { HandleLoadConfirmed();     });
    BindPanel("Settings", Panel_Settings, [this]() { HandleSettingsConfirmed(); });
    BindPanel("Quit",     Panel_Quit,     [this]() { HandleQuitConfirmed();     });

    NavigateTo("Resume");
}

void UPauseMenuWidget::RefreshOnOpen()
{
    SetCursorIndex(0);
    NavigateTo("Resume");
}

void UPauseMenuWidget::NavigateTo(FName PanelId)
{
    UMenuPanelBase** Found = NavigationMap.Find(PanelId);
    if (!Found || !*Found)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PauseMenuWidget] NavigateTo: unknown panel '%s'"), *PanelId.ToString());
        return;
    }

    if (!ActivePanelId.IsNone())
    {
        if (UMenuPanelBase** Current = NavigationMap.Find(ActivePanelId))
        {
            (*Current)->OnPanelDeactivated();
        }
    }

    PanelSwitcher->SetActiveWidget(*Found);
    ActivePanelId = PanelId;
    (*Found)->OnPanelActivated();

    // Keep cursor index in sync when NavigateTo is called externally (e.g. Cancel buttons)
    const int32 OrderIndex = PanelOrder.IndexOfByKey(PanelId);
    if (OrderIndex != INDEX_NONE && OrderIndex != CursorIndex)
    {
        SetCursorIndex(OrderIndex);
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
        {
            Audio->PlayUISoundEvent(EUISoundEvent::TabSwitch);
        }
    }
}

// ---------------------------------------------------------------------------
// Keyboard navigation
// ---------------------------------------------------------------------------

void UPauseMenuWidget::NavigateUp()
{
    if (PanelOrder.IsEmpty()) { return; }
    SetCursorIndex(FMath::Max(0, CursorIndex - 1));
    NavigateTo(PanelOrder[CursorIndex]);

    UE_LOG(LogTemp, Verbose,
        TEXT("[PauseMenuWidget] NavigateUp -> index %d (%s)"),
        CursorIndex, *PanelOrder[CursorIndex].ToString());
}

void UPauseMenuWidget::NavigateDown()
{
    if (PanelOrder.IsEmpty()) { return; }
    SetCursorIndex(FMath::Min(PanelOrder.Num() - 1, CursorIndex + 1));
    NavigateTo(PanelOrder[CursorIndex]);

    UE_LOG(LogTemp, Verbose,
        TEXT("[PauseMenuWidget] NavigateDown -> index %d (%s)"),
        CursorIndex, *PanelOrder[CursorIndex].ToString());
}

void UPauseMenuWidget::ConfirmSelection()
{
    UMenuPanelBase** Found = NavigationMap.Find(ActivePanelId);
    if (!Found || !*Found) { return; }

    (*Found)->ConfirmPanel();

    UE_LOG(LogTemp, Log,
        TEXT("[PauseMenuWidget] ConfirmSelection: confirmed panel '%s'"),
        *ActivePanelId.ToString());
}

bool UPauseMenuWidget::CancelSelection()
{
    if (ActivePanelId == "Resume")
    {
        // Already on Resume — signal controller to close the menu
        return false;
    }

    // Navigate back to Resume
    SetCursorIndex(0);
    NavigateTo("Resume");
    return true;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void UPauseMenuWidget::SetCursorIndex(int32 NewIndex)
{
    if (NewIndex == CursorIndex) { return; }

    BP_SetSidebarEntrySelected(CursorIndex, false);
    CursorIndex = NewIndex;
    BP_SetSidebarEntrySelected(CursorIndex, true);
}

void UPauseMenuWidget::BindPanel(FName PanelId, UMenuPanelBase* Panel, TFunction<void()> OnConfirm)
{
    if (!Panel)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PauseMenuWidget] BindPanel: panel '%s' is null — check BindWidget names in WBP_PauseMenu."),
            *PanelId.ToString());
        return;
    }

    NavigationMap.Add(PanelId, Panel);
    Panel->OnPanelConfirmed.AddLambda(OnConfirm);
    Panel->OnPanelNavigationRequested.AddLambda([this](FName TargetId)
    {
        NavigateTo(TargetId);
    });
}

UMyGameInstance* UPauseMenuWidget::GetGI() const
{
    return GetWorld() ? GetWorld()->GetGameInstance<UMyGameInstance>() : nullptr;
}

// ---------------------------------------------------------------------------
// Confirm handlers
// ---------------------------------------------------------------------------

void UPauseMenuWidget::HandleResumeConfirmed()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
        {
            Audio->PlayUISoundEvent(EUISoundEvent::ButtonConfirm);
        }
    }

    OnResumeRequested.Broadcast();
}

void UPauseMenuWidget::HandleSaveConfirmed()
{
    UMyGameInstance* GI = GetGI();
    if (!GI) { return; }

    if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
    {
        Audio->PlayUISoundEvent(EUISoundEvent::ButtonConfirm);
    }

    NavigateTo("Resume");
}

void UPauseMenuWidget::HandleLoadConfirmed()
{
    UMyGameInstance* GI = GetGI();
    if (!GI) { return; }

    // Request load/continue loads the save into memory. Now read the saved level.
    const FName SavedLevel = GI->GetActiveSavedLevelName();
    if (SavedLevel.IsNone())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[MainMenuWidget] HandleLoadConfirmed: no saved level after load — going to first level."));
        return;
    }

    if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
    {
        Audio->PlayUISoundEvent(EUISoundEvent::ButtonConfirm);
    }

    FLevelTransitionContext Context;
    Context.ToLevel = SavedLevel;
    GI->RequestLevelChange(SavedLevel, Context);
}

void UPauseMenuWidget::HandleSettingsConfirmed()
{
    UE_LOG(LogTemp, Log, TEXT("[PauseMenuWidget] Settings confirmed (applied by panel)."));
}

void UPauseMenuWidget::HandleQuitConfirmed()
{
    UMyGameInstance* GI = GetGI();
    if (GI)
    {
        GI->RequestSaveGame();

        if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
        {
            Audio->PlayUISoundEvent(EUISoundEvent::ButtonConfirm);
        }
    }

    APlayerController* PC = GetOwningPlayer();
    if (!PC) { return; }

    UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
}