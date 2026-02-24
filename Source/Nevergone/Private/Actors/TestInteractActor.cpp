// Copyright Xyzto Works


#include "Actors/TestInteractActor.h"
#include "Data/ActorSaveData.h"
#include "ActorComponents/InteractableComponent.h"
#include "ActorComponents/SaveableComponent.h"
#include "GameInstance/SaveKeys.h"


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
	FSavePayload Payload;

	FMemoryWriter Writer(Payload.Data, true);
	FArchive& Ar = Writer;

	int32 Temp = InteractCount;
	Ar << Temp;

	OutData.CustomDataMap.Add(SaveKeys::TestInteract, MoveTemp(Payload));
}

void ATestInteractActor::ReadSaveData_Implementation(const FActorSaveData& InData)
{
	const FSavePayload* Payload = InData.CustomDataMap.Find(SaveKeys::TestInteract);
	if (!Payload)
	{
		return;
	}

	FMemoryReader Reader(Payload->Data, true);
	FArchive& Ar = Reader;

	Ar << InteractCount;
}

// Called every frame
void ATestInteractActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

