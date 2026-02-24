#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/BattleInputContext.h"
#include "BattleInputManager.generated.h"

class ACharacterBase;
class ABattleCameraPawn;
class UTurnManager;
class IBattleInputReceiver;

class UCombatManager;

UCLASS()
class NEVERGONE_API UBattleInputManager : public UObject
{
	GENERATED_BODY()

public:
	/* ----- Initialization ----- */

	void Initialize(UTurnManager* InTurnManager, ABattleCameraPawn* InCameraPawn, UCombatManager* InCombatManager);

	void SetInputContext(const FBattleInputContext& NewContext);
	void SetSelectedCharacter(ACharacterBase* NewSelection);

	/* ----- PlayerController input ----- */

	void OnConfirmPressed();
	void OnCancelPressed();
	
	void OnEndTurn();
	
	void OnHover(const FVector& WorldLocation);
	void OnSelectNextUnit();
	void OnSelectPreviousUnit();
	void OnSelectNextAction();
	void OnSelectPreviousAction();

	void OnCameraMove(const FVector2D& Input);
	void OnCameraZoom(float Value);
	void OnCameraRotate(float Value);

	/* ----- Battle notifications ----- */

	void OnActiveUnitChanged(ACharacterBase* NewActiveUnit);
	
	/* ----- Input Config ----- */
	
	bool CanAcceptInput() const;

private:
	/* ----- Routing ----- */

	void RouteConfirm();
	void RouteCancel();
	void RouteHover(const FVector& WorldLocation);

	void RouteCameraMove(const FVector2D& Input);
	void RouteCameraZoom(float Value);
	void RouteCameraRotate(float Value);

	void CacheUnitInput(ACharacterBase* Unit);
	IBattleInputReceiver* ResolveReceiver() const;

private:
	
	UPROPERTY()
	UCombatManager* CombatManager = nullptr;
	
	UPROPERTY()
	UTurnManager* TurnManager = nullptr;

	UPROPERTY()
	ABattleCameraPawn* BattleCameraPawn = nullptr;

	UPROPERTY()
	ACharacterBase* SelectedCharacter = nullptr;

	UPROPERTY()
	FBattleInputContext InputContext;

	TScriptInterface<IBattleInputReceiver> SelectedCharacterInput;
	
	TScriptInterface<IBattleInputReceiver> DefaultInputReceiver;
};