// Copyright Xyzto Works


#include "Actors/BattlefieldEnemySpawnPoint.h"

// Sets default values
ABattlefieldEnemySpawnPoint::ABattlefieldEnemySpawnPoint()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

// Called when the game starts or when spawned
void ABattlefieldEnemySpawnPoint::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABattlefieldEnemySpawnPoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

