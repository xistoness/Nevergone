#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/BattleInputReceiver.h"
#include "BattleCameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class NEVERGONE_API ABattleCameraPawn
	: public APawn
	, public IBattleInputReceiver
{
	GENERATED_BODY()

protected:
	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere)
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere)
	UCameraComponent* Camera;

	FVector2D MovementInput = FVector2D::ZeroVector;
	float ZoomInput = 0.f;
	float RotationInput = 0.f;

	UPROPERTY(EditAnywhere, Category = "Camera|Movement")
	float PanSpeed = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float ZoomSpeed = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float MinZoom = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float MaxZoom = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "Camera|Rotation")
	float RotationSpeed = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Camera|Rotation")
	bool bAllowRotation = false;

	FBox BattlefieldBounds;

	bool bCameraInputEnabled = true;

	// One-shot focus
	bool bIsFocusing = false;
	FVector FocusTargetLocation;

	UPROPERTY(EditAnywhere, Category = "Camera|Focus")
	float FocusInterpSpeed = 5.0f;

	// Persistent actor lock
	UPROPERTY()
	TObjectPtr<AActor> LockedActor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Camera|Lock")
	FVector LockOffset = FVector(0.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "Camera|Lock")
	float LockInterpSpeed = 6.0f;

public:
	ABattleCameraPawn();

	void SetBattlefieldBounds(const FBox& InBounds);
	void FocusOnLocationSmooth(const FVector& WorldLocation);
	void FocusOnBounds(const FBox& Bounds);
	void ResetView();

	void LockOnActor(AActor* InActor);
	void ClearLockOnActor();
	bool HasLockedActor() const;

	// Returns true when the camera is close enough to its locked actor to be
	// considered "arrived" — used by CombatManager to gate AI action sequencing.
	bool IsNearLockedActor(float ToleranceSquared = 2500.f) const;

	virtual void Input_CameraMove(const FVector2D& Input) override;
	virtual void Input_CameraZoom(float Value) override;
	virtual void Input_CameraRotate(float Value) override;

	void EnableCameraInput();
	void DisableCameraInput();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void ClampToBounds();
	void ApplyMovement(float DeltaTime);
	void UpdateActorLock(float DeltaTime);
};