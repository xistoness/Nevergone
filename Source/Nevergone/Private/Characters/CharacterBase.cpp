// Copyright Xyzto Works


#include "Characters/CharacterBase.h"
#include "ActorComponents/InteractableComponent.h"
#include "ActorComponents/SaveableComponent.h"
#include "Data/ActorSaveData.h"
#include "GameInstance/MyGameInstance.h"
#include "EngineTools/CollisionChannels.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/SpringArmComponent.h"

// Sets default values
ACharacterBase::ACharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;
	
	SaveableComponent = CreateDefaultSubobject<USaveableComponent>(TEXT("SaveableComponent"));
}

void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
}

void ACharacterBase::WriteSaveData_Implementation(FActorSaveData& OutData) const
{	
	// Get Controller rotation
	FRotator TempControllerRot = ControllerRot;
	if (AController* Ctrl = GetController())
	{
		TempControllerRot = Ctrl->GetControlRotation();
	}

	// Get SpringArm length if dynamic zoom is allowed
	float TempArmLength = ArmLength;
	if (USpringArmComponent* Arm = GetCameraBoom())
	{
		TempArmLength = Arm->TargetArmLength;
	}

	// Serialize
	OutData.CustomData.Reset();
	FMemoryWriter Writer(OutData.CustomData, true);
	FArchive& Ar = Writer;

	Ar << TempArmLength;
	Ar << TempControllerRot;
}

void ACharacterBase::ReadSaveData_Implementation(const FActorSaveData& InData)
{
	if (InData.CustomData.Num() > 0)
	{
		FMemoryReader Reader(InData.CustomData, true);
		FArchive& Ar = Reader;

		Ar << ArmLength;
		Ar << ControllerRot;
	}
	
}

void ACharacterBase::OnPostRestore_Implementation()
{
	ResetCamera();
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


void ACharacterBase::OnInteractionPressed()
{
	UE_LOG(LogTemp, Warning, TEXT("Interagindo..."));
	FVector Start;
	FRotator Rot;

	GetController()->GetPlayerViewPoint(Start, Rot);

	FVector End = Start + (Rot.Vector() * 250.f);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		NevergoneCollision::Interactable,
		Params))
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			UInteractableComponent* InteractComp =
			HitActor->FindComponentByClass<UInteractableComponent>();

			if (InteractComp)
			{
				InteractComp->Interact(this);
			}
		}
	}
}

void ACharacterBase::OnSaveGamePressed()
{
	UE_LOG(LogTemp, Warning, TEXT("Save requested"));

	if (UMyGameInstance* GI = GetGameInstance<UMyGameInstance>())
	{
		GI->RequestSaveGame();
	}
}

void ACharacterBase::OnLoadGamePressed()
{
	UE_LOG(LogTemp, Warning, TEXT("Load requested"));

	if (UMyGameInstance* GI = GetGameInstance<UMyGameInstance>())
	{
		GI->RequestLoadGame();
	}
}

// Called every frame
void ACharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ACharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(InteractionInput, ETriggerEvent::Triggered, this, &ACharacterBase::OnInteractionPressed);
		EIC->BindAction(SaveGameInput, ETriggerEvent::Triggered, this, &ACharacterBase::OnSaveGamePressed);
		EIC->BindAction(LoadGameInput, ETriggerEvent::Triggered, this, &ACharacterBase::OnLoadGamePressed);
	}
}

