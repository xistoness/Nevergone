// Copyright Xyzto Works


#include "Actors/BattlefieldAllySpawnPoint.h"

// Sets default values
ABattlefieldAllySpawnPoint::ABattlefieldAllySpawnPoint()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

}

// Called when the game starts or when spawned
void ABattlefieldAllySpawnPoint::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABattlefieldAllySpawnPoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

