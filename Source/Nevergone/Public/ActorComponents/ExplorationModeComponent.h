// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "ActorComponents/CharacterModeComponent.h"
#include "Interfaces/ExplorationInputReceiver.h"
#include "ExplorationModeComponent.generated.h"

class ACharacter;
class UCharacterMovementComponent;
class UInteractableComponent;
class UNoiseDefinition;
class UExcursionThrowComponent;

// ---------------------------------------------------------------------------
// UExplorationModeComponent
//
// The survivor's whole moveset while exploring: moving, looking for something
// to interact with, crouching, sprinting and the stamina behind the sprint.
// AExplorationPlayerController feeds it through IExplorationInputReceiver.
//
// Stamina lives here and nowhere else. It fills up when the mode starts, is
// never saved, and does not survive the excursion: an excursion is a single
// session, so there is nothing to carry over. When it runs out the survivor
// only goes back to walking, with no speed penalty, but starts panting, and
// panting is a noise enemies can hear.
// ---------------------------------------------------------------------------
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class NEVERGONE_API UExplorationModeComponent : public UCharacterModeComponent, public IExplorationInputReceiver
{
	GENERATED_BODY()

public:	
	UExplorationModeComponent();
	
	virtual void TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual void EnterMode() override;
	virtual void ExitMode() override;

	virtual void Input_Move(float Right, float Forward) override;
	virtual void Input_JumpStart() override;
	virtual void Input_JumpStop() override;
	virtual void Input_SprintPressed() override;
	virtual void Input_SprintReleased() override;
	virtual void Input_CrouchPressed() override;
	virtual void Input_CrouchReleased() override;
	virtual void Input_InteractPressed() override;
	virtual void Input_InteractReleased() override;
	virtual void Input_ThrowPressed() override;
	virtual void Input_ThrowReleased() override;

	// Stamina left, from 0 to 1. For the HUD.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Exploration|Stamina")
	float GetStaminaFraction() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Exploration|Movement")
	bool IsSprinting() const { return bSprinting; }

	// Suspends the survivor's own input while a screen is open (dialogue,
	// inventory, container, pause menu). Driven by
	// AExplorationPlayerController from its input mode. Unlike ExitMode it
	// keeps the stance and the stamina: the survivor is still standing in
	// the same corridor, crouched or not.
	void SetInputSuspended(bool bSuspended);

protected:
	UPROPERTY()
	UInteractableComponent* HighlightedInteractable = nullptr;

	UPROPERTY(EditAnywhere, Category="Interaction")
	float MaxInteractionDistance = 200.0f;

	// How long interact must be held on a target that has a hold action
	// (the latch of a door) before it counts as a hold instead of a tap.
	UPROPERTY(EditAnywhere, Category="Interaction", meta = (ClampMin = "0.05", Units = "s"))
	float InteractHoldSeconds = 0.4f;

	// --- Movement ---

	UPROPERTY(EditAnywhere, Category="Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 300.0f;

	UPROPERTY(EditAnywhere, Category="Movement", meta = (ClampMin = "0.0"))
	float SprintSpeed = 550.0f;

	UPROPERTY(EditAnywhere, Category="Movement", meta = (ClampMin = "0.0"))
	float CrouchSpeed = 150.0f;

	// Capsule half height while crouched. Lower than the standing one is what
	// lets the survivor hide behind furniture from an enemy's line of sight.
	UPROPERTY(EditAnywhere, Category="Movement", meta = (ClampMin = "1.0"))
	float CrouchedHalfHeight = 55.0f;

	// --- Stamina ---

	UPROPERTY(EditAnywhere, Category="Movement|Stamina", meta = (ClampMin = "1.0"))
	float MaxStamina = 100.0f;

	UPROPERTY(EditAnywhere, Category="Movement|Stamina", meta = (ClampMin = "0.0"))
	float StaminaDrainPerSecond = 20.0f;

	UPROPERTY(EditAnywhere, Category="Movement|Stamina", meta = (ClampMin = "0.0"))
	float StaminaRecoveryPerSecond = 12.5f;

	// Time after the sprint ends before stamina starts coming back.
	UPROPERTY(EditAnywhere, Category="Movement|Stamina", meta = (ClampMin = "0.0"))
	float StaminaRecoveryDelaySeconds = 1.0f;

	// Stamina needed to start a sprint. Above zero on purpose: without it the
	// sprint would stutter on and off at the bottom of the bar.
	UPROPERTY(EditAnywhere, Category="Movement|Stamina", meta = (ClampMin = "0.0"))
	float MinStaminaToSprint = 15.0f;

	// --- Panting ---

	// Emitted repeatedly after stamina runs out. Loudness and sound live in
	// the asset, like every other noise.
	UPROPERTY(EditAnywhere, Category="Movement|Stamina")
	TObjectPtr<UNoiseDefinition> PantingNoise;

	UPROPERTY(EditAnywhere, Category="Movement|Stamina", meta = (ClampMin = "0.0"))
	float PantingDurationSeconds = 4.0f;

	UPROPERTY(EditAnywhere, Category="Movement|Stamina", meta = (ClampMin = "0.1"))
	float PantingIntervalSeconds = 1.0f;
	
	void UpdateInteractionTrace();

private:

	void UpdateStamina(float DeltaTime);

	void UpdatePanting(float DeltaTime);

	void StartPanting();

	void SetSprinting(bool bNewSprinting);

	// Pushes WalkSpeed or SprintSpeed into the movement component. The
	// crouched speed is handled by the engine through MaxWalkSpeedCrouched.
	void ApplyMovementSpeed();

	ACharacter* GetCharacter() const;

	UCharacterMovementComponent* GetMovement() const;

	// The sprint button is held. Whether the survivor is actually sprinting
	// is decided every tick by UpdateStamina.
	bool bSprintRequested = false;

	bool bSprinting = false;

	float Stamina = 0.0f;

	float RecoveryDelayLeft = 0.0f;

	float PantingTimeLeft = 0.0f;

	float NextPantingIn = 0.0f;

	bool bInputSuspended = false;

	// --- Hold interact ---

	// Counts a press held on a target with a hold action. Done when the
	// hold fires, the button is released (a tap) or the target changes.
	void UpdateInteractHold(float DeltaTime);

	void CancelInteractHold();

	// Target of the press being held, null when no hold is pending.
	UPROPERTY()
	TObjectPtr<UInteractableComponent> PendingHoldTarget = nullptr;

	float InteractHoldElapsed = 0.0f;

	// Sibling on the same survivor. Throw input is forwarded to it.
	UExcursionThrowComponent* GetThrowComponent() const;
};
