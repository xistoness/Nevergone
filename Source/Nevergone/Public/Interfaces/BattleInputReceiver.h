// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BattleInputReceiver.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UBattleInputReceiver : public UInterface
{
	GENERATED_BODY()
};


class NEVERGONE_API IBattleInputReceiver
{
	GENERATED_BODY()

public:
	/* ----- Core actions ----- */

	// Confirm current action (ability, selection, UI option, etc.)
	virtual void Input_Confirm() {}

	// Cancel current action or step back in context
	virtual void Input_Cancel() {}


	/* ----- Tactical commands ----- */
	virtual void Input_Hover(const FVector& WorldLocation) {}
	
	// Select next controllable unit (if applicable)
	virtual void Input_SelectNextAction() {}

	// Select previous controllable unit (if applicable)
	virtual void Input_SelectPreviousAction() {}


	/* ----- Navigation / selection ----- */

	// Select next controllable unit (if applicable)
	virtual void Input_SelectNextUnit() {}

	// Select previous controllable unit (if applicable)
	virtual void Input_SelectPreviousUnit() {}


	/* ----- Turn control ----- */

	// Explicit request to end the current turn
	virtual void Input_EndTurn() {}


	/* ----- Camera control (optional receivers) ----- */

	// Camera pan input
	virtual void Input_CameraMove(const FVector2D& Input) {}

	// Camera zoom input
	virtual void Input_CameraZoom(float Value) {}

	// Camera rotate input
	virtual void Input_CameraRotate(float Value) {}
};
