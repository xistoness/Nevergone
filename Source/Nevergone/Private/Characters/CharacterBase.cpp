// Copyright Xyzto Works


#include "Characters/CharacterBase.h"
#include "Actors/Interactable.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

// Sets default values
ACharacterBase::ACharacterBase()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void ACharacterBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	GetOrCreateGuid();
}

FGuid ACharacterBase::GetOrCreateGuid()
{
	if (!SaveGuid.IsValid())
	{
		SaveGuid = FGuid::NewGuid();
	}
	return SaveGuid;
}

void ACharacterBase::SetActorGuid(const FGuid& NewGuid)
{
	SaveGuid = NewGuid;
}

void ACharacterBase::WriteSaveData_Implementation(FActorSaveData& OutData) const
{
	ISaveable::WriteSaveData_Implementation(OutData);
	OutData.ActorGuid = SaveGuid;
	OutData.ActorClass = GetActorClass();
	OutData.Transform = GetActorTransform();
	OutData.LevelName = GetWorld()->GetOutermost()->GetFName();
}

void ACharacterBase::ReadSaveData(const FActorSaveData& InData)
{
	SaveGuid = InData.ActorGuid;
	SetActorTransform(InData.Transform);
}

TSoftClassPtr<AActor> ACharacterBase::GetActorClass() const
{
	return GetClass();
}

// Called when the game starts or when spawned
void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
}

void ACharacterBase::OnActionPressed()
{
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
		ECC_Visibility,
		Params))
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			if (HitActor->Implements<UInteractable>())
			{
				IInteractable::Execute_Interact(HitActor, this);
			}
		}
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
		EIC->BindAction(InteractionInput, ETriggerEvent::Triggered, this, &ACharacterBase::OnActionPressed);
	}
}

