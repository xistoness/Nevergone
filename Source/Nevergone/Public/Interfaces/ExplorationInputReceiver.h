// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ExplorationInputReceiver.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UExplorationInputReceiver : public UInterface
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// IExplorationInputReceiver
//
// Everything the player can do while exploring. AExplorationPlayerController
// owns the input actions and calls these; UExplorationModeComponent
// implements them. Because the component only exists in exploration mode,
// switching to battle removes these actions by construction, with no "am I in
// combat?" checks anywhere.
// ---------------------------------------------------------------------------
class NEVERGONE_API IExplorationInputReceiver
{
	GENERATED_BODY()

public:

	// Right and Forward are the raw axes of the move input.
	virtual void Input_Move(float Right, float Forward) = 0;

	virtual void Input_JumpStart() = 0;

	virtual void Input_JumpStop() = 0;

	virtual void Input_SprintPressed() = 0;

	virtual void Input_SprintReleased() = 0;

	// Toggles or starts the crouch, depending on the player's setting.
	virtual void Input_CrouchPressed() = 0;

	// Only ends the crouch when the player chose hold mode.
	virtual void Input_CrouchReleased() = 0;

	// A press interacts at once, unless the target does something when held
	// (IInteractable::CanInteractHold): then the release decides, a tap or a
	// hold. Without a release the press stays a tap.
	virtual void Input_InteractPressed() = 0;
	virtual void Input_InteractReleased() = 0;

	// Held to aim a distraction along its arc; releasing throws it.
	virtual void Input_ThrowPressed() = 0;
	virtual void Input_ThrowReleased() = 0;

	// Held to open the tool ring on the throwables; releasing chooses the
	// highlighted one. The world keeps running meanwhile.
	virtual void Input_ToolRingPressed() = 0;
	virtual void Input_ToolRingReleased() = 0;

	// While the tool ring is open: the mouse delta moves its pointer, the
	// gamepad stick position points at a slice. The ring's own input
	// context consumes both, so the camera does not turn meanwhile.
	virtual void Input_ToolRingPoint(const FVector2D& Delta) = 0;
	virtual void Input_ToolRingStick(const FVector2D& Stick) = 0;
};
