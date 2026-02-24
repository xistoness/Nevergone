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

UCLASS()
class NEVERGONE_API ABattlePlayerController : public ANevergonePlayerController
{
	GENERATED_BODY()

public:
	// Called when battle starts
	void EnterBattleMode(UCombatManager* InCombatManager);

protected:
	virtual void BeginPlay() override;
	void PreviewMovement(FIntPoint TargetCoord);
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
};