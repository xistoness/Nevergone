// Copyright Xyzto Works


#include "Characters/CharacterBase.h"
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
#include "Debug/DebugDrawHelper.h"
#include "GameFramework/CharacterMovementComponent.h"


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
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
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
}

void ACharacterBase::WriteSaveData_Implementation(FActorSaveData& OutData) const
{	
	FSavePayload Payload;

	// Controller rotation
	FRotator TempControllerRot = ControllerRot;
	if (AController* Ctrl = GetController())
	{
		TempControllerRot = Ctrl->GetControlRotation();
	}

	// Camera arm length
	float TempArmLength = ArmLength;
	if (USpringArmComponent* Arm = GetCameraBoom())
	{
		TempArmLength = Arm->TargetArmLength;
	}

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

void ACharacterBase::SetPendingMoveLocation(const FVector& InLocation)
{
	PendingMoveLocation = InLocation;
}

const FVector& ACharacterBase::GetPendingMoveLocation() const
{
	return PendingMoveLocation;
}

void ACharacterBase::MoveToLocation(const FVector& InLocation)
{
	SetActorLocation(InLocation);
}

UMyAbilitySystemComponent* ACharacterBase::GetAbilitySystemComponent()
{
	return AbilitySystemComponent;
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
