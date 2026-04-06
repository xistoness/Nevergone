// Copyright Xyzto Works

#include "Actors/BattlefieldSpawnZone.h"

ABattlefieldSpawnZone::ABattlefieldSpawnZone()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABattlefieldSpawnZone::BeginPlay()
{
	Super::BeginPlay();
}