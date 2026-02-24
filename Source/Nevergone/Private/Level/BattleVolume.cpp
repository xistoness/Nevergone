// Copyright Xyzto Works


#include "Level/BattleVolume.h"

ABattleVolume::ABattleVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	SetRootComponent(BoxComponent);

	BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TileSize   = 100.f;
	GridHeight = 500.f;
}

FVector ABattleVolume::GetOrigin() const
{
	return BoxComponent->GetComponentLocation();
}

FVector ABattleVolume::GetExtent() const
{
	return BoxComponent->GetScaledBoxExtent();
}

