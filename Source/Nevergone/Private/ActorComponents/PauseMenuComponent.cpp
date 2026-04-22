// Copyright Xyzto Works

#include "ActorComponents/PauseMenuComponent.h"

#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/PauseMenuWidget.h"


UPauseMenuComponent::UPauseMenuComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPauseMenuComponent::BindPauseInput(UEnhancedInputComponent* EIC, UInputAction* PauseAction)
{
    if (!EIC || !PauseAction) { return; }

    EIC->BindAction(PauseAction, ETriggerEvent::Triggered, this,
        &UPauseMenuComponent::TogglePause);

    UE_LOG(LogTemp, Log, TEXT("[PauseMenuComponent] Pause input bound."));
}

void UPauseMenuComponent::CreatePauseWidget()
{
    if (!PauseMenuWidgetClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PauseMenuComponent] PauseMenuWidgetClass not set — assign in BP defaults."));
        return;
    }

    if (PauseWidget) { return; }

    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC) { return; }

    PauseWidget = CreateWidget<UPauseMenuWidget>(PC, PauseMenuWidgetClass);
    if (!PauseWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[PauseMenuComponent] CreateWidget failed."));
        return;
    }

    PauseWidget->AddToViewport(200);
    PauseWidget->SetVisibility(ESlateVisibility::Hidden);
    PauseWidget->OnResumeRequested.AddUObject(this, &UPauseMenuComponent::Resume);

    UE_LOG(LogTemp, Log, TEXT("[PauseMenuComponent] PauseWidget created."));
}

void UPauseMenuComponent::TogglePause()
{
    if (bIsPaused) { ClosePause(); }
    else           { OpenPause();  }
}

void UPauseMenuComponent::Resume()
{
    ClosePause();
}

void UPauseMenuComponent::NavigateUp()
{
    if (PauseWidget) { PauseWidget->NavigateUp(); }
}

void UPauseMenuComponent::NavigateDown()
{
    if (PauseWidget) { PauseWidget->NavigateDown(); }
}

void UPauseMenuComponent::ConfirmSelection()
{
    if (PauseWidget) { PauseWidget->ConfirmSelection(); }
}

bool UPauseMenuComponent::CancelSelection()
{
    if (!PauseWidget) { return false; }

    if (!PauseWidget->CancelSelection())
    {
        // Widget says it's already on Resume — close the menu
        ClosePause();
        return false;
    }

    return true;
}

void UPauseMenuComponent::OpenPause()
{
    if (!PauseWidget) { return; }

    bIsPaused = true;
    PauseWidget->RefreshOnOpen();
    PauseWidget->SetVisibility(ESlateVisibility::Visible);

    // Notify controller to set CurrentInputMode = PauseMenu
    OnPauseOpened.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("[PauseMenuComponent] Pause menu opened."));
}

void UPauseMenuComponent::ClosePause()
{
    if (!PauseWidget) { return; }

    bIsPaused = false;
    PauseWidget->SetVisibility(ESlateVisibility::Hidden);

    // Notify controller to restore previous input mode
    OnPauseClosed.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("[PauseMenuComponent] Pause menu closed."));
}