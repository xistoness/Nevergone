// Copyright Xyzto Works

#include "Characters/PlayerControllers/ExplorationPlayerController.h"

#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Widgets/ExcursionResultWidget.h"
#include "World/ExcursionSubsystem.h"
#include "ActorComponents/ExplorationModeComponent.h"
#include "ActorComponents/PauseMenuComponent.h"
#include "Characters/CharacterBase.h"
#include "GameInstance/GameContextManager.h"
#include "Interfaces/ExplorationInputReceiver.h"
#include "Party/PartyManagerSubsystem.h"
#include "Widgets/DialogueWidget.h"
#include "Widgets/ExplorationPromptWidget.h"
#include "Widgets/InventoryWidget.h"
#include "Widgets/ContainerTransferWidget.h"
#include "Widgets/QuestLogWidget.h"
#include "Widgets/Party/PartyManagerWidget.h"
#include "Actors/ContainerActor.h"
#include "World/DialogueSubsystem.h"
#include "World/QuestSubsystem.h"

AExplorationPlayerController::AExplorationPlayerController()
{
    PauseMenuComponent = CreateDefaultSubobject<UPauseMenuComponent>(TEXT("PauseMenuComponent"));
}

void AExplorationPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EIC) { return; }

    if (LookInput)
        EIC->BindAction(LookInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::OnLook);

    if (MoveInput)
        EIC->BindAction(MoveInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::OnMove);

    if (JumpInput)
    {
        EIC->BindAction(JumpInput, ETriggerEvent::Started, this, &AExplorationPlayerController::OnJumpStart);
        EIC->BindAction(JumpInput, ETriggerEvent::Completed, this, &AExplorationPlayerController::OnJumpStop);
    }

    if (SprintInput)
    {
        EIC->BindAction(SprintInput, ETriggerEvent::Started, this, &AExplorationPlayerController::OnSprintStart);
        EIC->BindAction(SprintInput, ETriggerEvent::Completed, this, &AExplorationPlayerController::OnSprintStop);
    }

    if (CrouchInput)
    {
        EIC->BindAction(CrouchInput, ETriggerEvent::Started, this, &AExplorationPlayerController::OnCrouchStart);
        EIC->BindAction(CrouchInput, ETriggerEvent::Completed, this, &AExplorationPlayerController::OnCrouchStop);
    }

    if (ThrowInput)
    {
        // Canceled as well as Completed: either way the button is no longer
        // held, and the aim must not stay open.
        EIC->BindAction(ThrowInput, ETriggerEvent::Started, this, &AExplorationPlayerController::OnThrowStart);
        EIC->BindAction(ThrowInput, ETriggerEvent::Completed, this, &AExplorationPlayerController::OnThrowStop);
        EIC->BindAction(ThrowInput, ETriggerEvent::Canceled, this, &AExplorationPlayerController::OnThrowStop);
    }

    if (InteractionInput)
    {
        // Started is the press. With a Released trigger on the action (as
        // on the throw), Completed is the release and holding is possible;
        // with only Pressed, Completed follows the press at once and every
        // interaction is a tap.
        EIC->BindAction(InteractionInput, ETriggerEvent::Started, this, &AExplorationPlayerController::HandleInteract);
        EIC->BindAction(InteractionInput, ETriggerEvent::Completed, this, &AExplorationPlayerController::HandleInteractReleased);
    }

    if (QuestLogInput)
        EIC->BindAction(QuestLogInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleQuestLog);

    if (PartyManagerInput)
        EIC->BindAction(PartyManagerInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandlePartyManager);

    if (UIConfirmInput)
        EIC->BindAction(UIConfirmInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleUIConfirm);

    if (UICancelInput)
        EIC->BindAction(UICancelInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleUICancel);

    if (UIUpInput)
        EIC->BindAction(UIUpInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleUIUp);

    if (UIDownInput)
        EIC->BindAction(UIDownInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleUIDown);

    if (UILeftInput)
        EIC->BindAction(UILeftInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleUILeft);

    if (UIRightInput)
        EIC->BindAction(UIRightInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleUIRight);

    if (PauseInput)
        PauseMenuComponent->BindPauseInput(EIC, PauseInput);

    if (InventoryInput)
        EIC->BindAction(InventoryInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleInventory);

    if (InventoryPrevCharInput)
        EIC->BindAction(InventoryPrevCharInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleInventoryPrevChar);

    if (InventoryNextCharInput)
        EIC->BindAction(InventoryNextCharInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleInventoryNextChar);

    WarnAboutActionsNotTriggeredWhenPaused();
}

void AExplorationPlayerController::WarnAboutActionsNotTriggeredWhenPaused() const
{
    // Every action a screen listens to. Gameplay-only actions (look, move,
    // jump, sprint, crouch) must stay unflagged so they do nothing while
    // paused.
    const UInputAction* ScreenActions[] =
    {
        PauseInput, InteractionInput, QuestLogInput, PartyManagerInput, InventoryInput,
        UIConfirmInput, UICancelInput, UIUpInput, UIDownInput, UILeftInput, UIRightInput,
        InventoryPrevCharInput, InventoryNextCharInput,
    };

    for (const UInputAction* Action : ScreenActions)
    {
        if (Action && !Action->bTriggerWhenPaused)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[ExplorationPlayerController] %s: input action '%s' is used on screens, which pause the world, but does not have Trigger When Paused set; it will not respond while a screen is open."),
                *GetNameSafe(this), *GetNameSafe(Action));
        }
    }
}

// ---------------------------------------------------------------------------
// Input mode management
// ---------------------------------------------------------------------------

void AExplorationPlayerController::ApplyExplorationInputMode()
{
    CurrentInputMode = EExplorationInputMode::Gameplay;

    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);

    bShowMouseCursor       = false;
    bEnableClickEvents     = false;
    bEnableMouseOverEvents = false;

    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);

    UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] Input mode -> Gameplay"));

    SyncWorldWithInputMode();
}

void AExplorationPlayerController::ApplyDialogueInputMode()
{
    CurrentInputMode = EExplorationInputMode::Dialogue;

    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);

    bShowMouseCursor       = true;
    bEnableClickEvents     = true;
    bEnableMouseOverEvents = true;

    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);

    UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] Input mode -> Dialogue"));

    SyncWorldWithInputMode();
}

void AExplorationPlayerController::ApplyUIOverlayInputMode(EExplorationInputMode TargetMode)
{
    CurrentInputMode = TargetMode;

    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);

    bShowMouseCursor       = true;
    bEnableClickEvents     = true;
    bEnableMouseOverEvents = true;

    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);

    UE_LOG(LogTemp, Log,
        TEXT("[ExplorationPlayerController] Input mode -> UIOverlay (%d)"), (int32)TargetMode);

    SyncWorldWithInputMode();
}

bool AExplorationPlayerController::IsScreenMode(EExplorationInputMode Mode)
{
    return Mode != EExplorationInputMode::Gameplay && Mode != EExplorationInputMode::Capture;
}

void AExplorationPlayerController::ApplyCaptureInputMode()
{
    CurrentInputMode = EExplorationInputMode::Capture;

    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);

    bShowMouseCursor       = false;
    bEnableClickEvents     = false;
    bEnableMouseOverEvents = false;

    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);

    UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] %s input mode -> Capture"), *GetNameSafe(GetPawn()));

    SyncWorldWithInputMode();
}

void AExplorationPlayerController::EnterCaptureInputMode() { ApplyCaptureInputMode();     }
void AExplorationPlayerController::ExitCaptureInputMode()  { ApplyExplorationInputMode(); }

void AExplorationPlayerController::SyncWorldWithInputMode()
{
    // Pause and suspension are derived from the mode instead of being
    // toggled by each screen. That is what lets the pause menu open over a
    // dialogue and close back into a still-paused dialogue with no
    // bookkeeping: HandlePauseClosed restores the mode, and the mode decides.
    const bool bScreenOpen = IsScreenMode(CurrentInputMode);

    // Releases never reach the pawn outside Gameplay (see GetInputReceiver),
    // so the pawn drops its held sprint itself.
    if (UExplorationModeComponent* Mode = GetExplorationMode())
    {
        Mode->SetInputSuspended(CurrentInputMode != EExplorationInputMode::Gameplay);
    }

    if (IsPaused() == bScreenOpen)
    {
        return;
    }

    if (SetPause(bScreenOpen))
    {
        UE_LOG(LogTemp, Log,
            TEXT("[ExplorationPlayerController] %s world %s"),
            *GetNameSafe(GetPawn()), bScreenOpen ? TEXT("paused") : TEXT("resumed"));
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ExplorationPlayerController] %s: the game mode refused to %s the world"),
            *GetNameSafe(GetPawn()), bScreenOpen ? TEXT("pause") : TEXT("resume"));
    }
}

void AExplorationPlayerController::EnterDialogueInputMode()  { ApplyDialogueInputMode();    }
void AExplorationPlayerController::ExitDialogueInputMode()   { ApplyExplorationInputMode(); }

// ---------------------------------------------------------------------------
// Pause callbacks — fired by PauseMenuComponent delegates
// ---------------------------------------------------------------------------

void AExplorationPlayerController::HandlePauseOpened()
{
    // Close any open overlay before entering pause so the player never returns
    // to a stale widget state. Dialogue is exempt — pause mid-dialogue is allowed.
    CloseActiveOverlay();

    PrePauseInputMode = CurrentInputMode;
    ApplyUIOverlayInputMode(EExplorationInputMode::PauseMenu);

    UE_LOG(LogTemp, Log,
        TEXT("[ExplorationPlayerController] Pause opened — saved pre-pause mode: %d"),
        (int32)PrePauseInputMode);
}

void AExplorationPlayerController::HandlePauseClosed()
{
    switch (PrePauseInputMode)
    {
    case EExplorationInputMode::Dialogue:
        ApplyDialogueInputMode();
        break;
    case EExplorationInputMode::Capture:
        ApplyCaptureInputMode();
        break;
    case EExplorationInputMode::QuestLog:
    case EExplorationInputMode::PartyManager:
    case EExplorationInputMode::Inventory:
    case EExplorationInputMode::Container:
    case EExplorationInputMode::Result:
        ApplyUIOverlayInputMode(PrePauseInputMode);
        break;
    default:
        ApplyExplorationInputMode();
        break;
    }

    UE_LOG(LogTemp, Log,
        TEXT("[ExplorationPlayerController] Pause closed — restored mode: %d"),
        (int32)PrePauseInputMode);
}

// ---------------------------------------------------------------------------
// Gameplay inputs
// ---------------------------------------------------------------------------

void AExplorationPlayerController::OnLook(const FInputActionValue& Value)
{
    if (CurrentInputMode != EExplorationInputMode::Gameplay) { return; }

    const FVector2D LookAxis = Value.Get<FVector2D>();
    AddYawInput(LookAxis.X * MouseSensitivity);
    AddPitchInput(LookAxis.Y * MouseSensitivity);
}

IExplorationInputReceiver* AExplorationPlayerController::GetInputReceiver() const
{
    // Outside gameplay the UI owns the input, so no action reaches the pawn.
    if (CurrentInputMode != EExplorationInputMode::Gameplay) { return nullptr; }

    return GetExplorationMode();
}

UExplorationModeComponent* AExplorationPlayerController::GetExplorationMode() const
{
    APawn* MyPawn = GetPawn();

    return MyPawn ? MyPawn->FindComponentByClass<UExplorationModeComponent>() : nullptr;
}

void AExplorationPlayerController::OnMove(const FInputActionValue& Value)
{
    if (IExplorationInputReceiver* Actions = GetInputReceiver())
    {
        const FVector2D MoveAxis = Value.Get<FVector2D>();
        Actions->Input_Move(MoveAxis.X, MoveAxis.Y);
    }
}

void AExplorationPlayerController::OnJumpStart()
{
    if (IExplorationInputReceiver* Actions = GetInputReceiver())
    {
        Actions->Input_JumpStart();
    }
}

void AExplorationPlayerController::OnJumpStop()
{
    if (IExplorationInputReceiver* Actions = GetInputReceiver())
    {
        Actions->Input_JumpStop();
    }
}

void AExplorationPlayerController::OnThrowStart()
{
    if (IExplorationInputReceiver* Actions = GetInputReceiver())
    {
        Actions->Input_ThrowPressed();
    }
}

void AExplorationPlayerController::OnThrowStop()
{
    if (IExplorationInputReceiver* Actions = GetInputReceiver())
    {
        Actions->Input_ThrowReleased();
    }
}

void AExplorationPlayerController::OnSprintStart()
{
    if (IExplorationInputReceiver* Actions = GetInputReceiver())
    {
        Actions->Input_SprintPressed();
    }
}

void AExplorationPlayerController::OnSprintStop()
{
    if (IExplorationInputReceiver* Actions = GetInputReceiver())
    {
        Actions->Input_SprintReleased();
    }
}

void AExplorationPlayerController::OnCrouchStart()
{
    if (IExplorationInputReceiver* Actions = GetInputReceiver())
    {
        Actions->Input_CrouchPressed();
    }
}

void AExplorationPlayerController::OnCrouchStop()
{
    if (IExplorationInputReceiver* Actions = GetInputReceiver())
    {
        Actions->Input_CrouchReleased();
    }
}

void AExplorationPlayerController::HandleInteract()
{
    switch (CurrentInputMode)
    {
    case EExplorationInputMode::Dialogue:
    case EExplorationInputMode::PartyManager:
    case EExplorationInputMode::QuestLog:
    case EExplorationInputMode::Result:
        HandleUIConfirm();
        break;

    case EExplorationInputMode::Gameplay:
        if (IExplorationInputReceiver* Actions = GetInputReceiver())
        {
            Actions->Input_InteractPressed();
        }
        break;

    default:
        break;
    }
}

void AExplorationPlayerController::HandleInteractReleased()
{
    if (CurrentInputMode != EExplorationInputMode::Gameplay)
    {
        return;
    }

    if (IExplorationInputReceiver* Actions = GetInputReceiver())
    {
        Actions->Input_InteractReleased();
    }
}

// ---------------------------------------------------------------------------
// UI overlay toggles
// ---------------------------------------------------------------------------

void AExplorationPlayerController::HandleQuestLog()
{
    if (CurrentInputMode == EExplorationInputMode::Dialogue ||
        CurrentInputMode == EExplorationInputMode::PauseMenu ||
        CurrentInputMode == EExplorationInputMode::Capture ||
        CurrentInputMode == EExplorationInputMode::Result)
    {
        return;
    }

    if (!QuestLogWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPlayerController] HandleQuestLog: QuestLogWidget is null."));
        return;
    }

    if (CurrentInputMode == EExplorationInputMode::QuestLog)
    {
        QuestLogWidget->SetVisibility(ESlateVisibility::Hidden);
        ApplyExplorationInputMode();
        UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] QuestLog closed."));
    }
    else
    {
        CloseActiveOverlay();
        QuestLogWidget->RefreshQuestLog();
        QuestLogWidget->SetVisibility(ESlateVisibility::Visible);
        ApplyUIOverlayInputMode(EExplorationInputMode::QuestLog);
        UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] QuestLog opened."));
    }
}

void AExplorationPlayerController::HandlePartyManager()
{
    if (CurrentInputMode == EExplorationInputMode::Dialogue ||
        CurrentInputMode == EExplorationInputMode::PauseMenu ||
        CurrentInputMode == EExplorationInputMode::Capture ||
        CurrentInputMode == EExplorationInputMode::Result)
    {
        return;
    }

    if (!PartyManagerWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPlayerController] HandlePartyManager: PartyManagerWidget is null."));
        return;
    }

    if (CurrentInputMode == EExplorationInputMode::PartyManager)
    {
        PartyManagerWidget->SetVisibility(ESlateVisibility::Hidden);
        ApplyExplorationInputMode();
        UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] PartyManager closed."));
    }
    else
    {
        CloseActiveOverlay();
        PartyManagerWidget->RefreshPartyManager();
        PartyManagerWidget->SetVisibility(ESlateVisibility::Visible);
        ApplyUIOverlayInputMode(EExplorationInputMode::PartyManager);
        UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] PartyManager opened."));
    }
}

void AExplorationPlayerController::HandleInventory()
{
    if (CurrentInputMode == EExplorationInputMode::Dialogue ||
        CurrentInputMode == EExplorationInputMode::PauseMenu ||
        CurrentInputMode == EExplorationInputMode::Capture ||
        CurrentInputMode == EExplorationInputMode::Result)
    {
        return;
    }

    if (!InventoryWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPlayerController] HandleInventory: InventoryWidget is null."));
        return;
    }

    if (CurrentInputMode == EExplorationInputMode::Inventory)
    {
        InventoryWidget->CloseInventory();
        ApplyExplorationInputMode();
        UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] Inventory closed."));
    }
    else
    {
        CloseActiveOverlay();
        InventoryWidget->OpenInventory();
        ApplyUIOverlayInputMode(EExplorationInputMode::Inventory);
        UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] Inventory opened."));
    }
}

void AExplorationPlayerController::HandleInventoryPrevChar()
{
    if (CurrentInputMode != EExplorationInputMode::Inventory) return;
    if (InventoryWidget) InventoryWidget->SelectPreviousCharacter();
}

void AExplorationPlayerController::HandleInventoryNextChar()
{
    if (CurrentInputMode != EExplorationInputMode::Inventory) return;
    if (InventoryWidget) InventoryWidget->SelectNextCharacter();
}

// ---------------------------------------------------------------------------
// Unified UI navigation
// ---------------------------------------------------------------------------

void AExplorationPlayerController::HandleUIConfirm()
{
    switch (CurrentInputMode)
    {
    case EExplorationInputMode::Dialogue:
        if (DialogueWidget) { DialogueWidget->ConfirmCurrentChoice(); }
        break;
    case EExplorationInputMode::QuestLog:
        if (QuestLogWidget) { QuestLogWidget->ConfirmSelection(); }
        break;
    case EExplorationInputMode::PartyManager:
        if (PartyManagerWidget) { PartyManagerWidget->ConfirmSelection(); }
        break;
    case EExplorationInputMode::Inventory:
        if (InventoryWidget) { InventoryWidget->ConfirmSelection(); }
        break;
    case EExplorationInputMode::Container:
        if (ContainerTransferWidget) { ContainerTransferWidget->ConfirmSelection(); }
        break;
    case EExplorationInputMode::PauseMenu:
        PauseMenuComponent->ConfirmSelection();
        break;
    case EExplorationInputMode::Result:
        if (ExcursionResultWidget) { ExcursionResultWidget->Confirm(); }
        break;
    default:
        break;
    }
}

void AExplorationPlayerController::HandleUICancel()
{
    switch (CurrentInputMode)
    {
    case EExplorationInputMode::Dialogue:
        if (DialogueWidget) { DialogueWidget->CancelDialogue(); }
        break;

    case EExplorationInputMode::QuestLog:
        if (QuestLogWidget)
        {
            QuestLogWidget->SetVisibility(ESlateVisibility::Hidden);
            ApplyExplorationInputMode();
            UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] QuestLog closed via Cancel."));
        }
        break;

    case EExplorationInputMode::PartyManager:
        if (PartyManagerWidget)
        {
            if (!PartyManagerWidget->CancelSelection())
            {
                PartyManagerWidget->SetVisibility(ESlateVisibility::Hidden);
                ApplyExplorationInputMode();
                UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] PartyManager closed via Cancel."));
            }
        }
        break;

    case EExplorationInputMode::Inventory:
        if (InventoryWidget)
        {
            // CancelSelection closes context menu first (returns true).
            // On second press (nothing open) it returns false → close inventory.
            if (!InventoryWidget->CancelSelection())
            {
                InventoryWidget->CloseInventory();
                ApplyExplorationInputMode();
                UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] Inventory closed via Cancel."));
            }
        }
        break;

    case EExplorationInputMode::Container:
        if (ContainerTransferWidget)
        {
            // Same two-stage cancel as Inventory: first press dismisses the
            // quantity prompt if open, second press closes the transfer UI.
            if (!ContainerTransferWidget->CancelSelection())
            {
                ContainerTransferWidget->Close();
                ApplyExplorationInputMode();
                UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] Container closed via Cancel."));
            }
        }
        break;

    case EExplorationInputMode::PauseMenu:
        PauseMenuComponent->CancelSelection();
        break;

    default:
        break;
    }
}

void AExplorationPlayerController::HandleUIUp()
{
    switch (CurrentInputMode)
    {
    case EExplorationInputMode::Dialogue:
        if (DialogueWidget) { DialogueWidget->SelectPreviousChoice(); }
        break;
    case EExplorationInputMode::QuestLog:
        if (QuestLogWidget) { QuestLogWidget->NavigateUp(); }
        break;
    case EExplorationInputMode::PartyManager:
        if (PartyManagerWidget) { PartyManagerWidget->NavigateUp(); }
        break;
    case EExplorationInputMode::Inventory:
        if (InventoryWidget) { InventoryWidget->NavigateUp(); }
        break;
    case EExplorationInputMode::Container:
        if (ContainerTransferWidget) { ContainerTransferWidget->NavigateUp(); }
        break;
    case EExplorationInputMode::PauseMenu:
        PauseMenuComponent->NavigateUp();
        break;
    default:
        break;
    }
}

void AExplorationPlayerController::HandleUIDown()
{
    switch (CurrentInputMode)
    {
    case EExplorationInputMode::Dialogue:
        if (DialogueWidget) { DialogueWidget->SelectNextChoice(); }
        break;
    case EExplorationInputMode::QuestLog:
        if (QuestLogWidget) { QuestLogWidget->NavigateDown(); }
        break;
    case EExplorationInputMode::PartyManager:
        if (PartyManagerWidget) { PartyManagerWidget->NavigateDown(); }
        break;
    case EExplorationInputMode::Inventory:
        if (InventoryWidget) { InventoryWidget->NavigateDown(); }
        break;
    case EExplorationInputMode::Container:
        if (ContainerTransferWidget) { ContainerTransferWidget->NavigateDown(); }
        break;
    case EExplorationInputMode::PauseMenu:
        PauseMenuComponent->NavigateDown();
        break;
    default:
        break;
    }
}

void AExplorationPlayerController::HandleUILeft()
{
    if (CurrentInputMode == EExplorationInputMode::PartyManager)
    {
        if (PartyManagerWidget) { PartyManagerWidget->NavigateLeft(); }
    }
    else if (CurrentInputMode == EExplorationInputMode::Inventory)
    {
        if (InventoryWidget) { InventoryWidget->NavigateLeft(); }
    }
    else if (CurrentInputMode == EExplorationInputMode::Container)
    {
        if (ContainerTransferWidget) { ContainerTransferWidget->NavigateLeft(); }
    }
}

void AExplorationPlayerController::HandleUIRight()
{
    if (CurrentInputMode == EExplorationInputMode::PartyManager)
    {
        if (PartyManagerWidget) { PartyManagerWidget->NavigateRight(); }
    }
    else if (CurrentInputMode == EExplorationInputMode::Inventory)
    {
        if (InventoryWidget) { InventoryWidget->NavigateRight(); }
    }
    else if (CurrentInputMode == EExplorationInputMode::Container)
    {
        if (ContainerTransferWidget) { ContainerTransferWidget->NavigateRight(); }
    }
}

// ---------------------------------------------------------------------------
// CloseActiveOverlay
// ---------------------------------------------------------------------------

void AExplorationPlayerController::CloseActiveOverlay()
{
    // Closes whichever overlay widget is currently open and resets input mode
    // to Gameplay. Does NOT touch Dialogue (it has its own lifecycle) or
    // PauseMenu (only closeable via its own input).
    switch (CurrentInputMode)
    {
    case EExplorationInputMode::QuestLog:
        if (QuestLogWidget)
        {
            QuestLogWidget->SetVisibility(ESlateVisibility::Hidden);
            UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] CloseActiveOverlay: QuestLog closed."));
        }
        ApplyExplorationInputMode();
        break;

    case EExplorationInputMode::PartyManager:
        if (PartyManagerWidget)
        {
            PartyManagerWidget->SetVisibility(ESlateVisibility::Hidden);
            UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] CloseActiveOverlay: PartyManager closed."));
        }
        ApplyExplorationInputMode();
        break;

    case EExplorationInputMode::Inventory:
        if (InventoryWidget)
        {
            InventoryWidget->CloseInventory();
            UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] CloseActiveOverlay: Inventory closed."));
        }
        ApplyExplorationInputMode();
        break;

    case EExplorationInputMode::Container:
        if (ContainerTransferWidget)
        {
            ContainerTransferWidget->Close();
            UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] CloseActiveOverlay: Container closed."));
        }
        ApplyExplorationInputMode();
        break;

    default:
        // Gameplay, Dialogue, PauseMenu — nothing to close.
        break;
    }
}

// ---------------------------------------------------------------------------
// Widget creation
// ---------------------------------------------------------------------------

void AExplorationPlayerController::CreateInventoryWidget()
{
    if (!IsValid(InventoryWidgetClass) || InventoryWidget) { return; }

    InventoryWidget = CreateWidget<UInventoryWidget>(this, InventoryWidgetClass);
    if (!InventoryWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[ExplorationPlayerController] CreateInventoryWidget failed."));
        return;
    }

    // ZOrder 200 — above HUD elements (100) and dialogue (100), below pause menu (300+).
    InventoryWidget->AddToViewport(200);
    InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
    UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] InventoryWidget created."));
}

void AExplorationPlayerController::CreateContainerTransferWidget()
{
    if (!IsValid(ContainerTransferWidgetClass) || ContainerTransferWidget) { return; }

    ContainerTransferWidget = CreateWidget<UContainerTransferWidget>(this, ContainerTransferWidgetClass);
    if (!ContainerTransferWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[ExplorationPlayerController] CreateContainerTransferWidget failed."));
        return;
    }

    // Same ZOrder bracket as InventoryWidget — only one of them is visible at a time.
    ContainerTransferWidget->AddToViewport(200);
    ContainerTransferWidget->SetVisibility(ESlateVisibility::Collapsed);
    UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] ContainerTransferWidget created."));
}

void AExplorationPlayerController::CreateExplorationPromptWidget()
{
    if (!IsValid(ExplorationPromptWidgetClass) || ExplorationPromptWidget) { return; }

    ExplorationPromptWidget = CreateWidget<UExplorationPromptWidget>(this, ExplorationPromptWidgetClass);
    if (!ExplorationPromptWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[ExplorationPlayerController] CreateExplorationPromptWidget failed."));
        return;
    }

    // ZOrder 150 — above HUD elements (100) and dialogue (100), below interactive
    // overlays (200+). The prompt is non-blocking informational text, so it must
    // sit behind anything that captures input.
    ExplorationPromptWidget->AddToViewport(150);
    UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] ExplorationPromptWidget created."));
}

void AExplorationPlayerController::ShowExplorationPrompt(const FText& Message, float Duration)
{
    if (!ExplorationPromptWidget)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ExplorationPlayerController] ShowExplorationPrompt: widget not initialized — message '%s' dropped."),
            *Message.ToString());
        return;
    }

    UE_LOG(LogTemp, Verbose,
        TEXT("[ExplorationPlayerController] ShowExplorationPrompt: '%s' duration=%.2fs"),
        *Message.ToString(), Duration);

    ExplorationPromptWidget->ShowMessage(Message, Duration);
}

void AExplorationPlayerController::OpenContainerInteraction(AContainerActor* Container)
{
    if (!Container)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPlayerController] OpenContainerInteraction: null container."));
        return;
    }

    // Lazy-create on first open. The widget class field is editor-set on the
    // BP_ExplorationPlayerController; if it's missing we log and bail rather
    // than crash, mirroring the rest of the overlays' guarding pattern.
    if (!ContainerTransferWidget)
    {
        CreateContainerTransferWidget();
    }
    if (!ContainerTransferWidget)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[ExplorationPlayerController] OpenContainerInteraction: ContainerTransferWidgetClass not set on BP."));
        return;
    }

    CloseActiveOverlay();
    ContainerTransferWidget->OpenForContainer(Container);
    ApplyUIOverlayInputMode(EExplorationInputMode::Container);

    UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] Container '%s' opened."),
        *Container->GetName());
}

void AExplorationPlayerController::CreateDialogueWidget()
{
    if (!IsValid(DialogueWidgetClass) || DialogueWidget) { return; }

    DialogueWidget = CreateWidget<UDialogueWidget>(this, DialogueWidgetClass);
    if (!DialogueWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[ExplorationPlayerController] CreateDialogueWidget failed."));
        return;
    }

    DialogueWidget->AddToViewport(100);
    DialogueWidget->SetVisibility(ESlateVisibility::Hidden);
    UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] DialogueWidget created."));
}

void AExplorationPlayerController::CreateQuestLogWidget()
{
    if (!IsValid(QuestLogWidgetClass) || QuestLogWidget) { return; }

    QuestLogWidget = CreateWidget<UQuestLogWidget>(this, QuestLogWidgetClass);
    if (!QuestLogWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[ExplorationPlayerController] CreateQuestLogWidget failed."));
        return;
    }

    QuestLogWidget->AddToViewport(100);
    QuestLogWidget->SetVisibility(ESlateVisibility::Hidden);
    UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] QuestLogWidget created."));
}

void AExplorationPlayerController::CreatePartyManagerWidget()
{
    if (!IsValid(PartyManagerWidgetClass) || PartyManagerWidget) { return; }

    PartyManagerWidget = CreateWidget<UPartyManagerWidget>(this, PartyManagerWidgetClass);
    if (!PartyManagerWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[ExplorationPlayerController] CreatePartyManagerWidget failed."));
        return;
    }

    PartyManagerWidget->AddToViewport(100);
    PartyManagerWidget->SetVisibility(ESlateVisibility::Hidden);
    UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] CreatePartyManagerWidget created."));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void AExplorationPlayerController::BeginPlay()
{
    Super::BeginPlay();

    BindDialogueSubsystem();
    BindQuestSubsystem();
    BindExcursionSubsystem();

    PauseMenuComponent->OnPauseOpened.AddUObject(this, &AExplorationPlayerController::HandlePauseOpened);
    PauseMenuComponent->OnPauseClosed.AddUObject(this, &AExplorationPlayerController::HandlePauseClosed);
}

void AExplorationPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    UE_LOG(LogTemp, Verbose,
        TEXT("[ExplorationPlayerController] OnPossess -> %s"), *GetNameSafe(InPawn));

    ApplyDefaultInputMappings();

    if (ACharacterBase* PossessedCharacter = Cast<ACharacterBase>(InPawn))
    {
        PossessedCharacter->EnableExplorationMode();
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGameContextManager* ContextManager = GI->GetSubsystem<UGameContextManager>())
        {
            SetControlRotation(ContextManager->GetSavedExplorationControlRotation());
        }
    }

    CreateDialogueWidget();
    CreateQuestLogWidget();
    CreatePartyManagerWidget();
    CreateInventoryWidget();
    CreateContainerTransferWidget();
    CreateExplorationPromptWidget();
    PauseMenuComponent->CreatePauseWidget();

    if (DialogueSubsystem && DialogueSubsystem->IsDialogueActive())
    {
        ApplyDialogueInputMode();
    }
    else
    {
        ApplyExplorationInputMode();
    }
}

void AExplorationPlayerController::OnUnPossess()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[ExplorationPlayerController] OnUnPossess -> %s"), *GetNameSafe(GetPawn()));

    // The pawn must not keep this controller's screen state. Possessed again
    // in the same mode, ACharacterBase::SetMode skips EnterMode, so nothing
    // else would clear the suspension.
    if (UExplorationModeComponent* Mode = GetExplorationMode())
    {
        Mode->SetInputSuspended(false);
    }

    Super::OnUnPossess();
}

void AExplorationPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (DialogueSubsystem)
    {
        DialogueSubsystem->OnDialogueStarted.RemoveAll(this);
        DialogueSubsystem->OnDialogueEnded.RemoveAll(this);
    }

    if (UExcursionSubsystem* Excursion = UExcursionSubsystem::Get(this))
    {
        Excursion->OnExcursionEnded.RemoveAll(this);
    }

    // A controller destroyed with a screen open (a controller swap mid
    // menu) would otherwise leave the world paused with nobody to resume it.
    if (IsPaused())
    {
        SetPause(false);

        UE_LOG(LogTemp, Log,
            TEXT("[ExplorationPlayerController] %s ended play while paused; world resumed"),
            *GetNameSafe(this));
    }

    Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Subsystem binding
// ---------------------------------------------------------------------------

void AExplorationPlayerController::BindDialogueSubsystem()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) { return; }

    DialogueSubsystem = GI->GetSubsystem<UDialogueSubsystem>();
    if (!DialogueSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPlayerController] DialogueSubsystem not found."));
        return;
    }

    DialogueSubsystem->OnDialogueStarted.RemoveAll(this);
    DialogueSubsystem->OnDialogueEnded.RemoveAll(this);
    DialogueSubsystem->OnDialogueStarted.AddUObject(this, &AExplorationPlayerController::HandleDialogueStarted);
    DialogueSubsystem->OnDialogueEnded.AddUObject(this, &AExplorationPlayerController::HandleDialogueEnded);
}

void AExplorationPlayerController::BindQuestSubsystem()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) { return; }

    QuestSubsystem = GI->GetSubsystem<UQuestSubsystem>();
    if (!QuestSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPlayerController] QuestSubsystem not found."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] QuestSubsystem bound."));
}

void AExplorationPlayerController::BindExcursionSubsystem()
{
    UExcursionSubsystem* Excursion = UExcursionSubsystem::Get(this);

    if (!Excursion)
    {
        return;
    }

    Excursion->OnExcursionEnded.RemoveAll(this);
    Excursion->OnExcursionEnded.AddUObject(this, &AExplorationPlayerController::HandleExcursionEnded);
}

void AExplorationPlayerController::HandleExcursionEnded(const FExcursionResult& Result)
{
    if (!ExcursionResultWidget && ExcursionResultWidgetClass)
    {
        ExcursionResultWidget = CreateWidget<UExcursionResultWidget>(this, ExcursionResultWidgetClass);

        if (ExcursionResultWidget)
        {
            ExcursionResultWidget->OnContinue.AddUObject(this, &AExplorationPlayerController::HandleExcursionResultContinue);
        }
    }

    if (!ExcursionResultWidget)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[ExplorationPlayerController] %s: ExcursionResultWidgetClass not set; the excursion ended with no result screen"),
            *GetNameSafe(this));

        return;
    }

    CloseActiveOverlay();

    if (!ExcursionResultWidget->IsInViewport())
    {
        ExcursionResultWidget->AddToViewport();
    }

    ExcursionResultWidget->ShowResult(Result);

    // A screen like any other overlay: it pauses the world.
    ApplyUIOverlayInputMode(EExplorationInputMode::Result);
}

void AExplorationPlayerController::HandleExcursionResultContinue()
{
    UE_LOG(LogTemp, Log,
        TEXT("[ExplorationPlayerController] %s: result confirmed, restarting the excursion"),
        *GetNameSafe(this));

    if (UExcursionSubsystem* Excursion = UExcursionSubsystem::Get(this))
    {
        Excursion->RestartExcursion();
    }
}

void AExplorationPlayerController::HandleDialogueStarted() { EnterDialogueInputMode(); }
void AExplorationPlayerController::HandleDialogueEnded()   { ExitDialogueInputMode();  }
