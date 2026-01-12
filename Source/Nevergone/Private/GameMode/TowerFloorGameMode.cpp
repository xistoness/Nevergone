// Copyright Xyzto Works


#include "GameMode/TowerFloorGameMode.h"

ATowerFloorGameMode::ATowerFloorGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATowerFloorGameMode::BeginPlay()
{
	Super::BeginPlay();
	SetupFloor();
}

void ATowerFloorGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TeardownFloor();
	Super::EndPlay(EndPlayReason);
}

void ATowerFloorGameMode::SetupFloor()
{
	// Spawn or enable level-wide systems
	// Example: WorldManagerSubsystem initialization
}

void ATowerFloorGameMode::TeardownFloor()
{
	// Cleanup level-wide systems
}