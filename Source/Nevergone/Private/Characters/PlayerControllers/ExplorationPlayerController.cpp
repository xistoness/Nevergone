// Copyright Xyzto Works

#include "Characters/PlayerControllers/ExplorationPlayerController.h"

#include "EnhancedInputComponent.h"
#include "ActorComponents/ExplorationModeComponent.h"
#include "ActorComponents/PauseMenuComponent.h"
#include "Characters/CharacterBase.h"
#include "GameInstance/GameContextManager.h"
#include "Interfaces/ExplorationInputReceiver.h"
#include "Party/PartyManagerSubsystem.h"
#include "Widgets/DialogueWidget.h"
#include "Widgets/QuestLogWidget.h"
#include "Widgets/Party/PartyManagerWidget.h"
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

    if (InteractionInput)
        EIC->BindAction(InteractionInput, ETriggerEvent::Triggered, this, &AExplorationPlayerController::HandleInteract);

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
}

void AExplorationPlayerController::EnterDialogueInputMode()  { ApplyDialogueInputMode();    }
void AExplorationPlayerController::ExitDialogueInputMode()   { ApplyExplorationInputMode(); }

// ---------------------------------------------------------------------------
// Pause callbacks — fired by PauseMenuComponent delegates
// ---------------------------------------------------------------------------

void AExplorationPlayerController::HandlePauseOpened()
{
    // Save the current mode so we can restore it exactly on close.
    // This matters when pausing mid-dialogue or mid-overlay.
    PrePauseInputMode = CurrentInputMode;
    ApplyUIOverlayInputMode(EExplorationInputMode::PauseMenu);

    UE_LOG(LogTemp, Log,
        TEXT("[ExplorationPlayerController] Pause opened — saved pre-pause mode: %d"),
        (int32)PrePauseInputMode);
}

void AExplorationPlayerController::HandlePauseClosed()
{
    // Restore the mode that was active before pause.
    // Use the specific Apply* method for each mode to guarantee correct state.
    switch (PrePauseInputMode)
    {
    case EExplorationInputMode::Dialogue:
        ApplyDialogueInputMode();
        break;
    case EExplorationInputMode::QuestLog:
    case EExplorationInputMode::PartyManager:
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

void AExplorationPlayerController::HandleInteract()
{
    switch (CurrentInputMode)
    {
    case EExplorationInputMode::Dialogue:
    case EExplorationInputMode::PartyManager:
    case EExplorationInputMode::QuestLog:
        HandleUIConfirm();
        break;

    case EExplorationInputMode::Gameplay:
        if (APawn* MyPawn = GetPawn())
        {
            if (IExplorationInputReceiver* Actions = MyPawn->FindComponentByClass<UExplorationModeComponent>())
            {
                Actions->Input_Interact();
            }
        }
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// UI overlay toggles
// ---------------------------------------------------------------------------

void AExplorationPlayerController::HandleQuestLog()
{
    if (CurrentInputMode == EExplorationInputMode::Dialogue ||
        CurrentInputMode == EExplorationInputMode::PauseMenu)
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
        if (CurrentInputMode == EExplorationInputMode::PartyManager && PartyManagerWidget)
        {
            PartyManagerWidget->SetVisibility(ESlateVisibility::Hidden);
        }

        QuestLogWidget->RefreshQuestLog();
        QuestLogWidget->SetVisibility(ESlateVisibility::Visible);
        ApplyUIOverlayInputMode(EExplorationInputMode::QuestLog);
        UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] QuestLog opened."));
    }
}

void AExplorationPlayerController::HandlePartyManager()
{
    if (CurrentInputMode == EExplorationInputMode::Dialogue ||
        CurrentInputMode == EExplorationInputMode::PauseMenu)
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
        if (CurrentInputMode == EExplorationInputMode::QuestLog && QuestLogWidget)
        {
            QuestLogWidget->SetVisibility(ESlateVisibility::Hidden);
        }

        PartyManagerWidget->RefreshPartyManager();
        PartyManagerWidget->SetVisibility(ESlateVisibility::Visible);
        ApplyUIOverlayInputMode(EExplorationInputMode::PartyManager);
        UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] PartyManager opened."));
    }
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
    case EExplorationInputMode::PauseMenu:
        PauseMenuComponent->ConfirmSelection();
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

    case EExplorationInputMode::PauseMenu:
        // CancelSelection on the component either navigates back to Resume
        // or closes the menu — both cases handled internally.
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
}

void AExplorationPlayerController::HandleUIRight()
{
    if (CurrentInputMode == EExplorationInputMode::PartyManager)
    {
        if (PartyManagerWidget) { PartyManagerWidget->NavigateRight(); }
    }
}

// ---------------------------------------------------------------------------
// Widget creation
// ---------------------------------------------------------------------------

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
    UE_LOG(LogTemp, Log, TEXT("[ExplorationPlayerController] PartyManagerWidget created."));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void AExplorationPlayerController::BeginPlay()
{
    Super::BeginPlay();

    BindDialogueSubsystem();
    BindQuestSubsystem();

    // Subscribe to pause delegates so this controller manages CurrentInputMode.
    PauseMenuComponent->OnPauseOpened.AddUObject(this, &AExplorationPlayerController::HandlePauseOpened);
    PauseMenuComponent->OnPauseClosed.AddUObject(this, &AExplorationPlayerController::HandlePauseClosed);
}

void AExplorationPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    UE_LOG(LogTemp, Warning,
        TEXT("[ExplorationPlayerController] OnPossess -> %s"), *GetNameSafe(InPawn));

    ApplyDefaultInputMappings();

    if (ACharacterBase* PossessedCharacter = Cast<ACharacterBase>(InPawn))
    {
        PossessedCharacter->ApplyCharacterInputMapping();
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
    Super::OnUnPossess();
}

void AExplorationPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (DialogueSubsystem)
    {
        DialogueSubsystem->OnDialogueStarted.RemoveAll(this);
        DialogueSubsystem->OnDialogueEnded.RemoveAll(this);
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

void AExplorationPlayerController::HandleDialogueStarted() { EnterDialogueInputMode(); }
void AExplorationPlayerController::HandleDialogueEnded()   { ExitDialogueInputMode();  }