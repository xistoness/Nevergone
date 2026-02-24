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

	// Root
	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root;

	// Camera boom
	UPROPERTY(VisibleAnywhere)
	USpringArmComponent* SpringArm;

	// Camera
	UPROPERTY(VisibleAnywhere)
	UCameraComponent* Camera;

	// Cached per-frame input
	FVector2D MovementInput = FVector2D::ZeroVector;
	float ZoomInput = 0.f;
	float RotationInput = 0.f;

	// Movement tuning
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

	// Battlefield limits
	FBox BattlefieldBounds;

	bool bCameraInputEnabled = true;
	
	// Focus state
	bool bIsFocusing = false;

	FVector FocusTargetLocation;
	
	UPROPERTY(EditAnywhere, Category = "Camera|Focus")
	float FocusInterpSpeed = 5.0f;

public:
	ABattleCameraPawn();

	// Camera control API (non-input)
	void SetBattlefieldBounds(const FBox& InBounds);
	void FocusOnLocationSmooth(const FVector& WorldLocation);
	void FocusOnBounds(const FBox& Bounds);
	void ResetView();
	
	/* ----- IBattleInputReceiver (camera) ----- */

	virtual void Input_CameraMove(const FVector2D& Input) override;
	virtual void Input_CameraZoom(float Value) override;
	virtual void Input_CameraRotate(float Value) override;

	void EnableCameraInput();
	void DisableCameraInput();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;



	// Internal helpers
	void ClampToBounds();
	void ApplyMovement(float DeltaTime);
};