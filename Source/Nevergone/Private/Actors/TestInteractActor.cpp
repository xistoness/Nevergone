// Copyright Xyzto Works


#include "Actors/TestInteractActor.h"
#include "Data/ActorSaveData.h"
#include "ActorComponents/InteractableComponent.h"
#include "ActorComponents/SaveableComponent.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"


// Sets default values
ATestInteractActor::ATestInteractActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SaveableComponent = CreateDefaultSubobject<USaveableComponent>(TEXT("SaveableComponent"));
	InteractableComponent = CreateDefaultSubobject<UInteractableComponent>(TEXT("InteractableComponent"));

}

void ATestInteractActor::Interact_Implementation(AActor* Interactor)
{
	InteractCount++;
	UE_LOG(LogTemp, Warning, TEXT("Interagiu %d vezes com %s"), InteractCount, *GetName());
}

// Called when the game starts or when spawned
void ATestInteractActor::BeginPlay()
{
	Super::BeginPlay();
	
}

void ATestInteractActor::WriteSaveData_Implementation(FActorSaveData& OutData) const
{
	OutData.CustomData.Reset();

	FMemoryWriter Writer(OutData.CustomData, true);
	FArchive& Ar = Writer;
	
	int32 Temp = InteractCount;
	Ar << Temp;
}

void ATestInteractActor::ReadSaveData_Implementation(const FActorSaveData& InData)
{
	if (InData.CustomData.Num() > 0)
	{
		FMemoryReader Reader(InData.CustomData, true);
		FArchive& Ar = Reader;
		
		Ar << InteractCount;
	}
}

// Called every frame
void ATestInteractActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

