// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "NevergonePlayerController.h"
#include "ExplorationPlayerController.generated.h"

class UPauseMenuComponent;
class UQuestSubsystem;
class UQuestLogWidget;
class UDialogueWidget;
class UDialogueSubsystem;
class UPartyManagerWidget;
class UPartyManagerSubsystem;
class UInputAction;
class UInventoryWidget;
class UContainerTransferWidget;
class UExplorationPromptWidget;
class AContainerActor;
class IExplorationInputReceiver;
class UExplorationModeComponent;
class UExcursionResultWidget;
struct FExcursionResult;

/**
 * Defines the current input state of the exploration mode.
 */
UENUM(BlueprintType)
enum class EExplorationInputMode : uint8
{
    Gameplay,
    Dialogue,
    QuestLog,
    PartyManager,
    PauseMenu,
    Inventory,
    Container,

    // An enemy caught the survivor. Input is blocked but the world is not
    // paused: the capture plays, frozen around the survivor.
    Capture,

    // The excursion ended and its result is on screen.
    Result,
};

/**
 * AExplorationPlayerController
 * Handles input mapping and UI state transitions for exploration gameplay.
 */
UCLASS()
class NEVERGONE_API AExplorationPlayerController : public ANevergonePlayerController
{
    GENERATED_BODY()

public:
    // --- Lifecycle & Initialization ---

    AExplorationPlayerController();

    virtual void SetupInputComponent() override;

    virtual void OnPossess(APawn* InPawn) override;

    virtual void OnUnPossess() override;

    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // --- Input Mode Management ---

    /** Restores input to standard gameplay (Game Only). */
    void ApplyExplorationInputMode();

    /** Switches input to dialogue mode (Game and UI). */
    void ApplyDialogueInputMode();

    /** Switches input to a specific UI overlay mode. */
    void ApplyUIOverlayInputMode(EExplorationInputMode TargetMode);

    void EnterDialogueInputMode();

    void ExitDialogueInputMode();

    /** Blocks the survivor's input for a capture. Called by UExcursionEncounterSubsystem. */
    void EnterCaptureInputMode();

    /** Gives input back after a capture the survivor walked away from. */
    void ExitCaptureInputMode();

    // --- Gameplay Input Actions ---

    /** Handles look/mouse movement. */
    void OnLook(const FInputActionValue& Value);

    /** Handles movement. Routed to the pawn's exploration component. */
    void OnMove(const FInputActionValue& Value);

    void OnJumpStart();

    void OnJumpStop();

    void OnSprintStart();

    void OnSprintStop();

    void OnCrouchStart();

    void OnCrouchStop();

    void OnThrowStart();

    void OnThrowStop();

    // --- Input Configuration (Properties) ---

    UPROPERTY(EditDefaultsOnly, Category = "Input|Gameplay", meta = (ToolTip = "Action for looking around."))
    UInputAction* LookInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|Gameplay", meta = (ToolTip = "Action for moving the survivor."))
    UInputAction* MoveInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|Gameplay", meta = (ToolTip = "Action for jumping."))
    UInputAction* JumpInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|Gameplay", meta = (ToolTip = "Action for sprinting. Held, not toggled."))
    UInputAction* SprintInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|Gameplay", meta = (ToolTip = "Action for crouching. Toggle or hold, per the player's setting."))
    UInputAction* CrouchInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|Gameplay", meta = (ToolTip = "Action for throwing a distraction. Held to aim, released to throw."))
    UInputAction* ThrowInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|Gameplay", meta = (ToolTip = "Action for interacting with objects/NPCs."))
    UInputAction* InteractionInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI", meta = (ToolTip = "Action for toggling the Quest Log."))
    UInputAction* QuestLogInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI", meta = (ToolTip = "Action for toggling the Party Manager."))
    UInputAction* PartyManagerInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI", meta = (ToolTip = "Action for opening the Pause Menu."))
    UInputAction* PauseInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI", meta = (ToolTip = "Action for toggling the Inventory."))
    UInputAction* InventoryInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI|Navigation")
    UInputAction* UIConfirmInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI|Navigation")
    UInputAction* UICancelInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI|Navigation")
    UInputAction* UIUpInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI|Navigation")
    UInputAction* UIDownInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI|Navigation")
    UInputAction* UILeftInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI|Navigation")
    UInputAction* UIRightInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI|Inventory")
    UInputAction* InventoryPrevCharInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI|Inventory")
    UInputAction* InventoryNextCharInput;

protected:
    // --- Protected State ---

    /** The possessed pawn's exploration component, or null outside gameplay. */
    IExplorationInputReceiver* GetInputReceiver() const;

    /** The possessed pawn's exploration component, whatever the input mode. */
    UExplorationModeComponent* GetExplorationMode() const;

    UPROPERTY(EditAnywhere, Category = "Input|Look")
    float MouseSensitivity = 0.75f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input")
    EExplorationInputMode CurrentInputMode = EExplorationInputMode::Gameplay;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pause")
    EExplorationInputMode PrePauseInputMode = EExplorationInputMode::Gameplay;

    // --- UI Components ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pause")
    UPauseMenuComponent* PauseMenuComponent;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UDialogueWidget> DialogueWidgetClass;

    UPROPERTY()
    UDialogueWidget* DialogueWidget = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UQuestLogWidget> QuestLogWidgetClass;

    UPROPERTY()
    UQuestLogWidget* QuestLogWidget = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UPartyManagerWidget> PartyManagerWidgetClass;

    UPROPERTY()
    UPartyManagerWidget* PartyManagerWidget = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UInventoryWidget> InventoryWidgetClass;

    UPROPERTY()
    UInventoryWidget* InventoryWidget = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UContainerTransferWidget> ContainerTransferWidgetClass;

    UPROPERTY()
    UContainerTransferWidget* ContainerTransferWidget = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UExplorationPromptWidget> ExplorationPromptWidgetClass;

    UPROPERTY()
    UExplorationPromptWidget* ExplorationPromptWidget = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UExcursionResultWidget> ExcursionResultWidgetClass;

    UPROPERTY()
    TObjectPtr<UExcursionResultWidget> ExcursionResultWidget = nullptr;

public:

    // Called by AContainerActor::Interact_Implementation to hand off the
    // open-the-UI step. Centralized here so AContainerActor stays decoupled
    // from widget creation and input mode handling — the controller already
    // owns those concerns for every other overlay.
    void OpenContainerInteraction(AContainerActor* Container);

    // Generic transient-message API for the exploration HUD.
    // Routes through the persistent UExplorationPromptWidget so any actor or
    // subsystem can show one-off feedback ("It's locked.", "Inventory full",
    // "Picked up X") without owning a widget. Safe to call on every frame —
    // the widget replaces the message and restarts its timer on each call.
    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowExplorationPrompt(const FText& Message, float Duration = 2.0f);

private:
    // --- Internal Input Handlers ---

    void HandleInteract();
    void HandleQuestLog();
    void HandlePartyManager();
    void HandleInventory();
    void HandleInventoryPrevChar();
    void HandleInventoryNextChar();
    void HandleUIConfirm();
    void HandleUICancel();
    void HandleUIUp();
    void HandleUIDown();
    void HandleUILeft();
    void HandleUIRight();
    void HandlePauseOpened();
    void HandlePauseClosed();

    // --- Internal Helpers ---

    // Suspends the pawn's own input outside Gameplay and pauses the world
    // while a screen is open. Called by every Apply*InputMode.
    void SyncWorldWithInputMode();

    // Every mode except Gameplay and Capture is a screen, and screens pause.
    static bool IsScreenMode(EExplorationInputMode Mode);

    void ApplyCaptureInputMode();

    // Screens run on a paused world, where Enhanced Input only fires actions
    // flagged Trigger When Paused. Logs every screen action missing the flag.
    void WarnAboutActionsNotTriggeredWhenPaused() const;

    // Closes whichever overlay is currently open (QuestLog, PartyManager, Inventory).
    // Called before opening any new overlay and before opening the pause menu.
    // Does NOT close Dialogue or PauseMenu.
    void CloseActiveOverlay();

    void CreateDialogueWidget();
    void CreateQuestLogWidget();
    void CreatePartyManagerWidget();
    void CreateInventoryWidget();
    void CreateContainerTransferWidget();
    void CreateExplorationPromptWidget();

    void BindDialogueSubsystem();

    // Shows the result screen when UExcursionSubsystem ends the excursion.
    void BindExcursionSubsystem();
    void HandleExcursionEnded(const FExcursionResult& Result);
    void HandleExcursionResultContinue();
    void BindQuestSubsystem();
    void HandleDialogueStarted();
    void HandleDialogueEnded();

    UPROPERTY()
    UDialogueSubsystem* DialogueSubsystem = nullptr;

    UPROPERTY()
    UQuestSubsystem* QuestSubsystem = nullptr;
};