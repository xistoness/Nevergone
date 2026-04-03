// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "NevergonePlayerController.h"
#include "InputActionValue.h"
#include "BattlePlayerController.generated.h"

class ABattleCameraPawn;
class UCombatManager;
class UBattleInputManager;
class UGridManager;
class UInputMappingContext;
class UInputAction;
class UActionHotbar;
class UBattleHUDWidget;

UCLASS()
class NEVERGONE_API ABattlePlayerController : public ANevergonePlayerController
{
	GENERATED_BODY()

public:
	// Called when battle starts
	void EnterBattleMode(UCombatManager* InCombatManager);

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	void SpawnPreviewActors();
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	

	/* ---------- Input mapping ---------- */

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Input")
	UInputMappingContext* BattleInputContext;

	/* ---------- Actions ---------- */

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Input|Actions")
	UInputAction* ConfirmAction;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Input|Actions")
	UInputAction* CancelAction;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Input|Actions")
	UInputAction* MoveCommandAction;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Input|Actions")
	UInputAction* SelectNextAction;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Input|Actions")
	UInputAction* SelectPreviousAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Battle|Input|Actions")
	UInputAction* SelectNextUnit;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Input|Actions")
	UInputAction* SelectPreviousUnit;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Input|Actions")
	UInputAction* EndTurnAction;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Input|Actions")
	UInputAction* CameraMoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Input|Actions")
	UInputAction* CameraZoomAction;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Input|Actions")
	UInputAction* CameraRotateAction;
	
	/* ---------- Input Preview ---------- */
	
	bool bHasValidPreview;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preview")
	TSubclassOf<AActor> PreviewActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preview")
	TSubclassOf<AActor> PreviewBlockedActorClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview")
	AActor* PreviewActor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview")
	AActor* PreviewBlockedActor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview")
	AActor* PreviewUIActor;

	/* ---------- Input handlers ---------- */

	void HandleConfirm();
	void HandleCancel();
	void HandleHover();
	void HandleSelectNextUnit();
	void HandleSelectPreviousUnit();
	void HandleSelectNextAction();
	void HandleSelectPreviousAction();
	void HandleEndTurn();
	void HandleCameraMove(const FInputActionValue& Value);
	void HandleCameraZoom(const FInputActionValue& Value);
	void HandleCameraRotate(const FInputActionValue& Value);

private:
	/* ---------- External managers ---------- */

	UPROPERTY()
	UCombatManager* CombatManager;

	UPROPERTY()
	UBattleInputManager* BattleInputManager;

	/* ---------- Camera ---------- */

	UPROPERTY()
	ABattleCameraPawn* BattleCameraPawn;

	UPROPERTY(EditDefaultsOnly, Category = "Battle|Camera")
	TSubclassOf<ABattleCameraPawn> BattleCameraPawnClass;

	void SpawnAndPossessBattleCamera();


	// -----------------------------------------------------------------------
	// Battle HUD
	// -----------------------------------------------------------------------

	/**
	 * Class to instantiate for the action hotbar.
	 * Set this to WBP_ActionHotbar in the Blueprint subclass of CombatManager's
	 * owning GameMode or in a data asset — whichever pattern you use to configure
	 * the manager. If you keep CombatManager as a plain UObject (no Blueprint
	 * subclass), expose this via the GameMode or GameInstance instead and pass
	 * it in before calling StartCombat.
	 */
	
	void CreateAndInitializeHotbar();
	void DestroyHotbar();

	void CreateAndInitializeHUD();
	void DestroyHUD();
	
	UPROPERTY(EditDefaultsOnly, Category = "Battle|HUD")
	TSubclassOf<UActionHotbar> ActionHotbarClass;

	/** Widget class for the master battle HUD (HP bars, turn indicator, results). */
	UPROPERTY(EditDefaultsOnly, Category = "Battle|HUD")
	TSubclassOf<UBattleHUDWidget> BattleHUDClass;

	/** Live hotbar widget instance. Owned by the viewport for its lifetime. */
	UPROPERTY()
	TObjectPtr<UActionHotbar> ActionHotbar;

	/** Live battle HUD instance. */
	UPROPERTY()
	TObjectPtr<UBattleHUDWidget> BattleHUD;
};