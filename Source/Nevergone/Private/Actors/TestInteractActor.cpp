// Copyright Xyzto Works


#include "Actors/TestInteractActor.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"


// Sets default values
ATestInteractActor::ATestInteractActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
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
	Super::WriteSaveData_Implementation(OutData);

	OutData.CustomData.Reset();

	FMemoryWriter Writer(OutData.CustomData, true);
	FArchive& Ar = Writer;
	
	int32 Temp = InteractCount;
	Ar << Temp;
}

void ATestInteractActor::ReadSaveData(const FActorSaveData& InData)
{
	Super::ReadSaveData(InData);
	
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

