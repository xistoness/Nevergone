// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/BattleTypes.h"
#include "Types/BattleInputContext.h"
#include "BattleHUDWidget.generated.h"

class UCombatManager;
class UCombatEventBus;
class UTurnManager;
class ACharacterBase;
class UBattleState;
class UUnitHPBarWidget;
class UTurnIndicatorWidget;
class UBattleResultsWidget;
class UCanvasPanel;

/**
 * Master in-battle HUD.
 *
 * owns a full-screen CanvasPanel (HPBarCanvas) where all HP bar widgets
 * are placed as children. NativeTick projects each unit's world position
 * to screen space and updates the corresponding CanvasPanelSlot every frame.
 * This avoids all tick/positioning issues with standalone viewport widgets.
 *
 * Setup in WBP_BattleHUD:
 *   - Root widget must be a CanvasPanel sized to fill the screen (Anchors: full)
 *   - Add a child CanvasPanel named "HPBarCanvas" (also full-screen, no hit-test)
 *   - Add a child VerticalBox named "TurnIndicatorContainer" anchored where you want
 *     the turn banner (e.g. top-center)
 */
UCLASS(Abstract)
class NEVERGONE_API UBattleHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void InitializeWithCombatManager(UCombatManager* InCombatManager);

	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void Deinitialize();

protected:

	// -----------------------------------------------------------------------
	// Widget class references — set in the Blueprint subclass defaults
	// -----------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Classes")
	TSubclassOf<UUnitHPBarWidget> HPBarWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Classes")
	TSubclassOf<UTurnIndicatorWidget> TurnIndicatorClass;

	// -----------------------------------------------------------------------
	// Named widgets — bind in the UMG designer
	// -----------------------------------------------------------------------

	/**
	 * Full-screen canvas that contains all HP bar widgets.
	 * Must exist in the WBP and be named exactly "HPBarCanvas".
	 * Set its Visibility to "Hit Test Invisible" so it doesn't eat input.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> HPBarCanvas;

	// -----------------------------------------------------------------------
	// Blueprint events
	// -----------------------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle HUD")
	void OnHUDInitialized();

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle HUD")
	void OnTurnChanged(bool bIsPlayerTurn);

	// -----------------------------------------------------------------------
	// Tick — updates all HP bar positions every frame
	// -----------------------------------------------------------------------

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** World-space Z offset above the actor origin where HP bars appear. */
	UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Layout")
	float HPBarWorldZOffset = 200.f;

	/** Desired size of each HP bar widget in the canvas (pixels). */
	UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Layout")
	FVector2D HPBarSize = FVector2D(120.f, 16.f);

private:

	void SpawnHPBars(UBattleState* BattleState);
	void SpawnTurnIndicator(UTurnManager* InTurnManager);

	UFUNCTION()
	void HandleCombatFinished(EBattleUnitTeam WinningTeam);

	void HandleTurnStateChanged(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase);

	UPROPERTY()
	TObjectPtr<UCombatManager> CombatManager;

	UPROPERTY()
	TObjectPtr<UTurnIndicatorWidget> TurnIndicatorInstance;

	// Parallel arrays — index matches between them
	UPROPERTY()
	TArray<TObjectPtr<ACharacterBase>> HPBarUnits;

	UPROPERTY()
	TArray<TObjectPtr<UUnitHPBarWidget>> HPBarWidgets;

	FDelegateHandle TurnStateHandle;
};