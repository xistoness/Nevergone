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

UENUM(BlueprintType)
enum class EExplorationInputMode : uint8
{
    Gameplay,
    Dialogue,
    QuestLog,
    PartyManager,
    PauseMenu,
};

UCLASS()
class NEVERGONE_API AExplorationPlayerController : public ANevergonePlayerController
{
    GENERATED_BODY()

public:

    AExplorationPlayerController();

    virtual void SetupInputComponent() override;

    void ApplyExplorationInputMode();
    void ApplyDialogueInputMode();
    void ApplyUIOverlayInputMode(EExplorationInputMode TargetMode);

    void EnterDialogueInputMode();
    void ExitDialogueInputMode();

    void OnLook(const FInputActionValue& Value);

    // --- Gameplay inputs ---
    UPROPERTY(EditDefaultsOnly, Category = "Input|Gameplay")
    UInputAction* LookInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|Gameplay")
    UInputAction* InteractionInput;

    // --- UI overlay toggles ---
    UPROPERTY(EditDefaultsOnly, Category = "Input|UI")
    UInputAction* QuestLogInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI")
    UInputAction* PartyManagerInput;

    UPROPERTY(EditDefaultsOnly, Category = "Input|UI")
    UInputAction* PauseInput;

    // --- Unified UI navigation ---
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

protected:

    UPROPERTY(EditAnywhere, Category = "Input|Look")
    float MouseSensitivity = 0.75f;

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pause")
    UPauseMenuComponent* PauseMenuComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input")
    EExplorationInputMode CurrentInputMode = EExplorationInputMode::Gameplay;

    // The mode active before the pause menu opened — restored on close.
    EExplorationInputMode PrePauseInputMode = EExplorationInputMode::Gameplay;

    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

    void HandleInteract();
    void HandleQuestLog();
    void HandlePartyManager();
    void HandleUIConfirm();
    void HandleUICancel();
    void HandleUIUp();
    void HandleUIDown();
    void HandleUILeft();
    void HandleUIRight();

    void HandlePauseOpened();
    void HandlePauseClosed();

    void CreateDialogueWidget();
    void CreateQuestLogWidget();
    void CreatePartyManagerWidget();

    void BindDialogueSubsystem();
    void BindQuestSubsystem();
    void HandleDialogueStarted();
    void HandleDialogueEnded();

    UPROPERTY()
    UDialogueSubsystem* DialogueSubsystem = nullptr;

    UPROPERTY()
    UQuestSubsystem* QuestSubsystem = nullptr;
};