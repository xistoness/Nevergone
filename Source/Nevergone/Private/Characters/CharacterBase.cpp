// Copyright Xyzto Works


#include "Characters/CharacterBase.h"

#include "AITestsCommon.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "ActorComponents/CharacterModeComponent.h"
#include "ActorComponents/SaveableComponent.h"
#include "Data/ActorSaveData.h"
#include "GameInstance/SaveKeys.h"
#include "ActorComponents/BattleModeComponent.h"
#include "ActorComponents/ExplorationModeComponent.h"
#include "ActorComponents/MyAbilitySystemComponent.h"

#include "GameFramework/SpringArmComponent.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Debug/DebugDrawHelper.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/FloatingCombatTextWidget.h"


ACharacterBase::ACharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;
	
	UnitStatsComponent = CreateDefaultSubobject<UUnitStatsComponent>(TEXT("UnitStatsComponent"));
	SaveableComponent = CreateDefaultSubobject<USaveableComponent>(TEXT("SaveableComponent"));
	ExplorationMode = CreateDefaultSubobject<UExplorationModeComponent>(TEXT("ExplorationMode"));
	BattleMode      = CreateDefaultSubobject<UBattleModeComponent>(TEXT("BattleMode"));
	AbilitySystemComponent = CreateDefaultSubobject<UMyAbilitySystemComponent>(TEXT("AbilitySystem"));
	
	
	ExplorationMode->SetComponentTickEnabled(false);
	BattleMode->SetComponentTickEnabled(false);
	
	GetCameraBoom()->bUsePawnControlRotation = true;
	
	ArmLength = 400.f;
	
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	
	SelectionIndicator = CreateDefaultSubobject<UDecalComponent>(TEXT("SelectionIndicator"));
	SelectionIndicator->SetupAttachment(RootComponent);

	// Size of the projected decal
	SelectionIndicator->DecalSize = FVector(64.f, 128.f, 128.f);

	// Rotate so it faces the ground
	SelectionIndicator->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

	// Hidden by default
	SelectionIndicator->SetVisibility(false);
}

void ACharacterBase::EnableExplorationMode()
{
	UE_LOG(LogTemp, Warning, TEXT("Setting exploration mode..."));
	SetMode(ExplorationMode);
}

void ACharacterBase::EnableBattleMode()
{
	UE_LOG(LogTemp, Warning, TEXT("Setting battle mode..."));
	SetMode(BattleMode);
}

void ACharacterBase::SetSelected(bool bSelected)
{
	SelectionIndicator->SetVisibility(bSelected);
}

void ACharacterBase::SetMode(UCharacterModeComponent* NewMode)
{
	if (ActiveMode == NewMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("Character changing to SAME mode"));
		return;
	}

	if (ActiveMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("Character exiting previous mode: %s"), *ActiveMode->GetName());
		ActiveMode->ExitMode();
	}

	ActiveMode = NewMode;

	if (ActiveMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("Character entering new mode: %s"), *ActiveMode->GetName());
		ActiveMode->EnterMode();
	}
}

void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

void ACharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bIsMovingAlongPath)
	{
		TickPathMove(DeltaTime);
	}
}

void ACharacterBase::BeginDestroy()
{
	UE_LOG(LogTemp, Error, TEXT("[BaseCharacter]: BeginDestroy -> %s"), *GetNameSafe(this));
	Super::BeginDestroy();
}

void ACharacterBase::Destroyed()
{
	UE_LOG(LogTemp, Error, TEXT("[BaseCharacter]: Destroyed -> %s"), *GetNameSafe(this));
	Super::Destroyed();
}

void ACharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogTemp, Error, TEXT("[BaseCharacter]: EndPlay -> %s | Reason=%d"), *GetNameSafe(this), (int32)EndPlayReason);
	Super::EndPlay(EndPlayReason);
}

void ACharacterBase::UnPossessed()
{
	UE_LOG(LogTemp, Warning, TEXT("[BaseCharacter]: UnPossessed -> %s"), *GetNameSafe(this));
	Super::UnPossessed();
}

void ACharacterBase::WriteSaveData_Implementation(FActorSaveData& OutData) const
{	
	FSavePayload Payload;

	FRotator TempControllerRot = ControllerRot;
	if (AController* Ctrl = GetController())
	{
		TempControllerRot = Ctrl->GetControlRotation();
	}

	float TempArmLength = ArmLength;
	if (USpringArmComponent* Arm = GetCameraBoom())
	{
		TempArmLength = Arm->TargetArmLength;
	}

	UE_LOG(LogTemp, Warning, TEXT("[BaseCharacter]: Saving BoomTargetArm=%s"),
		*FString::SanitizeFloat(TempArmLength));

	FMemoryWriter Writer(Payload.Data, true);
	FArchive& Ar = Writer;

	Ar << TempArmLength;
	Ar << TempControllerRot;

	OutData.CustomDataMap.Add(SaveKeys::Actor, MoveTemp(Payload));
}

void ACharacterBase::ReadSaveData_Implementation(const FActorSaveData& InData)
{
	const FSavePayload* Payload = InData.CustomDataMap.Find(SaveKeys::Actor);
	if (!Payload)
	{
		return;
	}

	FMemoryReader Reader(Payload->Data, true);
	FArchive& Ar = Reader;

	Ar << ArmLength;
	Ar << ControllerRot;
	
}

void ACharacterBase::OnPostRestore_Implementation()
{
	ResetCamera();
}

UUnitStatsComponent* ACharacterBase::GetUnitStats() const
{
	return UnitStatsComponent;
}

UBattleModeComponent* ACharacterBase::GetBattleModeComponent() const
{
	return BattleMode;
}

void ACharacterBase::MoveToLocation(const FVector& InLocation, const TArray<FVector>& WorldPoints)
{
	MoveAlongPath_Lerped(WorldPoints);
	//SetActorLocation(InLocation);
}

void ACharacterBase::SpawnFloatingText(const FString& Text, EFloatingTextType Type, UTexture2D* Icon)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !FloatingTextClass)
	{
		return;
	}

	UFloatingCombatTextWidget* Widget = CreateWidget<UFloatingCombatTextWidget>(PC, FloatingTextClass);
	if (!Widget)
	{
		return;
	}

	// Add to the viewport before Init so Tick is already active
	// when OnFloatingTextReady() triggers the animation in the Blueprint
	Widget->AddToViewport();
	Widget->SetAlignmentInViewport(FVector2D(0.5f, 1.f)); // anchor at the bottom-center of the text
	Widget->InitFloatingText(GetActorLocation(), Text, Type, Icon);
}

void ACharacterBase::MoveAlongPath_Lerped(const TArray<FVector>& WorldPoints)
{
	StopPathMove();

	if (WorldPoints.Num() == 0)
	{
		return;
	}

	PendingPathPoints = WorldPoints;
	CurrentPathIndex = 0;
	bIsMovingAlongPath = true;
}

void ACharacterBase::TickPathMove(float DeltaSeconds)
{
	if (!PendingPathPoints.IsValidIndex(CurrentPathIndex))
	{
		StopPathMove();
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector Target = PendingPathPoints[CurrentPathIndex];

	const float Dist = FVector::Dist(CurrentLocation, Target);
	if (Dist <= AcceptRadius)
	{
		SetActorLocation(Target);

		CurrentPathIndex++;
		if (!PendingPathPoints.IsValidIndex(CurrentPathIndex))
		{
			FinishPathMove();
		}
		return;
	}

	const FVector NewLocation =
		FMath::VInterpConstantTo(CurrentLocation, Target, DeltaSeconds, MoveSpeedUnitsPerSec);

	SetActorLocation(NewLocation);
}

void ACharacterBase::StopPathMove()
{
	bIsMovingAlongPath = false;
	PendingPathPoints.Reset();
	CurrentPathIndex = INDEX_NONE;
}

void ACharacterBase::FinishPathMove()
{
	bIsMovingAlongPath = false;
	PendingPathPoints.Reset();
	CurrentPathIndex = INDEX_NONE;
	OnPathMoveFinished.Broadcast();
	UE_LOG(LogTemp, Warning, TEXT("[BaseCharacter]: Character broadcasting OnPathMoveFinished"));
}

UMyAbilitySystemComponent* ACharacterBase::GetAbilitySystemComponent()
{
	return AbilitySystemComponent;
}

bool ACharacterBase::IsInExplorationMode()
{
	if (ActiveMode == ExplorationMode)
	{
		return true;
	}
	return false;
}

bool ACharacterBase::IsInBattleMode()
{
	if (ActiveMode == BattleMode)
	{
		return true;
	}
	return false;
}

void ACharacterBase::ResetCamera()
{
	if (USpringArmComponent* Arm = GetCameraBoom())
	{
		Arm->TargetArmLength = ArmLength;
	}

	if (AController* Ctrl = GetController())
	{
		Ctrl->SetControlRotation(ControllerRot);
	}
}

FVector ACharacterBase::GetGroundAlignedLocation(const FVector& GroundLocation) const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule)
	{
		return GroundLocation;
	}

	FVector AdjustedLocation = GroundLocation;
	AdjustedLocation.Z += Capsule->GetScaledCapsuleHalfHeight();
	return AdjustedLocation;
}