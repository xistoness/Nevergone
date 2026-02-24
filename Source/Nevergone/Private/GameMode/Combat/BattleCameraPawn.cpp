// Copyright Xyzto Works

#include "GameMode/Combat/BattleCameraPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

ABattleCameraPawn::ABattleCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SetActorRotation(FRotator(0.f, 180.f, 0.f));
	
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	SpringArm->TargetArmLength = 1800.0f;
	SpringArm->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
	SpringArm->bDoCollisionTest = false;
	
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll  = false;
	SpringArm->bInheritYaw   = true;
	
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

void ABattleCameraPawn::BeginPlay()
{
	Super::BeginPlay();
}

void ABattleCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ApplyMovement(DeltaTime);

	if (bIsFocusing)
	{
		const FVector NewLocation = FMath::VInterpTo(
			GetActorLocation(),
			FocusTargetLocation,
			DeltaTime,
			FocusInterpSpeed
		);

		SetActorLocation(NewLocation);

		if (FVector::DistSquared(NewLocation, FocusTargetLocation) < 10.f)
		{
			SetActorLocation(FocusTargetLocation);
			bIsFocusing = false;
		}
	}

	ClampToBounds();

	MovementInput = FVector2D::ZeroVector;
	ZoomInput = 0.f;
	RotationInput = 0.f;
}

void ABattleCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// Input is intentionally handled by BattlePlayerController
}

void ABattleCameraPawn::SetBattlefieldBounds(const FBox& InBounds)
{
	BattlefieldBounds = InBounds;
}

void ABattleCameraPawn::FocusOnLocationSmooth(const FVector& WorldLocation)
{
	UE_LOG(LogTemp, Warning, TEXT("[BattleCameraPawn] Focus on location smooth"));
	FocusTargetLocation = WorldLocation;
	bIsFocusing = true;
}


void ABattleCameraPawn::FocusOnBounds(const FBox& Bounds)
{
	SetActorLocation(Bounds.GetCenter());
}

void ABattleCameraPawn::ResetView()
{
	SpringArm->TargetArmLength = (MinZoom + MaxZoom) * 0.5f;
	SetActorRotation(FRotator::ZeroRotator);
}

void ABattleCameraPawn::EnableCameraInput()
{
	bCameraInputEnabled = true;
}

void ABattleCameraPawn::DisableCameraInput()
{
	bCameraInputEnabled = false;
}

/* ----- IBattleInputReceiver ----- */

void ABattleCameraPawn::Input_CameraMove(const FVector2D& Input)
{
	MovementInput = Input;
}

void ABattleCameraPawn::Input_CameraZoom(float Value)
{
	ZoomInput = Value;
}

void ABattleCameraPawn::Input_CameraRotate(float Value)
{
	if (bAllowRotation)
	{
		RotationInput = Value;
	}
}

/* ----- Internal ----- */

void ABattleCameraPawn::ClampToBounds()
{
	if (!BattlefieldBounds.IsValid)
	{
		return;
	}

	FVector Location = GetActorLocation();
	Location.X = FMath::Clamp(Location.X, BattlefieldBounds.Min.X, BattlefieldBounds.Max.X);
	Location.Y = FMath::Clamp(Location.Y, BattlefieldBounds.Min.Y, BattlefieldBounds.Max.Y);

	SetActorLocation(Location);
}

void ABattleCameraPawn::ApplyMovement(float DeltaTime)
{
	if (!bCameraInputEnabled)
	{
		return;
	}
	
	if (!MovementInput.IsNearlyZero() || 
	!FMath::IsNearlyZero(RotationInput))
	{
		bIsFocusing = false;
	}

	if (!MovementInput.IsNearlyZero())
	{
		const FRotator YawRotation(0.f, GetActorRotation().Yaw, 0.f);

		const FVector Forward =
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		const FVector Right =
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		const FVector Move =
			(Forward * MovementInput.Y + Right * MovementInput.X) *
			PanSpeed * DeltaTime;

		AddActorWorldOffset(Move, true);
	}

	if (!FMath::IsNearlyZero(ZoomInput))
	{
		SpringArm->TargetArmLength = FMath::Clamp(
			SpringArm->TargetArmLength - (ZoomInput * ZoomSpeed),
			MinZoom,
			MaxZoom
		);
	}

	if (!FMath::IsNearlyZero(RotationInput))
	{
		AddActorWorldRotation(
			FRotator(0.f, RotationInput * RotationSpeed * DeltaTime, 0.f)
		);
	}
}
