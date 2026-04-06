// Copyright Xyzto Works

#include "Characters/PlayerControllers/BattleResultsController.h"

#include "GameInstance/GameContextManager.h"
#include "GameMode/BattleResultsContext.h"
#include "Widgets/Combat/BattleResultsWidget.h"

void ABattleResultsController::BeginPlay()
{
    Super::BeginPlay();

    // Show cursor and block all game input — this screen is UI-only.
    bShowMouseCursor = true;
    SetInputMode(FInputModeUIOnly());
}

void ABattleResultsController::SetResultsContext(UBattleResultsContext* InContext)
{
    if (!InContext)
    {
        UE_LOG(LogTemp, Error, TEXT("[BattleResultsController] SetResultsContext: null context"));
        return;
    }

    ResultsContext = InContext;

    // Widget may already exist if BeginPlay ran first; create it now if not.
    ShowResultsWidget();
}

void ABattleResultsController::ShowResultsWidget()
{
    if (!ResultsContext) { return; }
    if (!ResultsWidgetClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[BattleResultsController] ResultsWidgetClass not set — assign in BP CDO"));
        return;
    }

    // CreateWidget requires a valid LocalPlayer — verify the swap completed correctly.
    if (!GetLocalPlayer())
    {
        UE_LOG(LogTemp, Error,
            TEXT("[BattleResultsController] No LocalPlayer — controller swap may not have completed"));
        return;
    }

    ResultsWidget = CreateWidget<UBattleResultsWidget>(this, ResultsWidgetClass);
    if (!ResultsWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[BattleResultsController] CreateWidget failed for ResultsWidget"));
        return;
    }

    ResultsWidget->OnContinueClicked.AddUObject(
        this, &ABattleResultsController::HandleContinueClicked);

    ResultsWidget->AddToViewport();

    const FBattleSessionData& Snap = ResultsContext->SessionSnapshot;
    const int32 EnemiesKilled = Snap.TotalEnemies - Snap.SurvivingEnemies;
    ResultsWidget->ShowResults(
        Snap.WinningTeam,
        Snap.SurvivingAllies,
        Snap.SurvivingEnemies,
        EnemiesKilled
    );

    UE_LOG(LogTemp, Log, TEXT("[BattleResultsController] Results widget shown"));
}

void ABattleResultsController::HandleContinueClicked()
{
    UE_LOG(LogTemp, Log,
        TEXT("[BattleResultsController] Continue clicked — requesting return to exploration"));

    UGameContextManager* ContextManager =
        GetGameInstance()->GetSubsystem<UGameContextManager>();

    if (ContextManager)
    {
        ContextManager->ReturnToExploration();
    }
}