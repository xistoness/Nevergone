// Copyright Xyzto Works

#include "Actors/ExcursionScentBait.h"

#include "Nevergone.h"
#include "Actors/ExcursionThrowable.h"
#include "Data/ItemDefinition.h"
#include "World/ExcursionScentSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AExcursionScentBait::AExcursionScentBait()
{
	PrimaryActorTick.bCanEverTick = true;

	// Stands where the flight body stopped, like the pickup the mesh is
	// copied from, so the mesh keeps the pickup's placement.
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	// Blocks nothing and cannot be aimed at: enemies walk up to it, the
	// player cannot pick it up again.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
}

void AExcursionScentBait::Initialize(const UItemDefinition* InItem)
{
	Item = InItem;

	if (!Item)
	{
		return;
	}

	ScentRadius = Item->ScentRadius;
	EatSeconds = Item->ScentEatSeconds;

	Life.Start(Item->ScentGroundSeconds);

	if (!AExcursionThrowable::CopyPickupLooks(Item, Mesh, this))
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionScentBait] %s: %s has no WorldActorClass with a PickupMesh; the bait is invisible"),
			*GetNameSafe(this), *GetNameSafe(Item));
	}
}

void AExcursionScentBait::BeginPlay()
{
	Super::BeginPlay();

	if (!Item)
	{
		UE_LOG(LogNevergone, Error,
			TEXT("[ExcursionScentBait] %s spawned without an item; removed"),
			*GetNameSafe(this));

		Destroy();

		return;
	}

	if (UExcursionScentSubsystem* Scent = UExcursionScentSubsystem::Get(this))
	{
		Scent->RegisterScentBait(this);
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionScentBait] %s of %s placed at %s: scent radius %.0f, %.1fs on the ground, %.1fs to eat"),
		*GetNameSafe(this), *GetNameSafe(Item), *GetActorLocation().ToCompactString(),
		ScentRadius, Life.GetSecondsLeft(), EatSeconds);
}

void AExcursionScentBait::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UExcursionScentSubsystem* Scent = UExcursionScentSubsystem::Get(this))
	{
		Scent->UnregisterScentBait(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AExcursionScentBait::NotifyEatingStarted(const AActor* Eater)
{
	if (Life.StartEating(EatSeconds))
	{
		UE_LOG(LogNevergone, Log,
			TEXT("[ExcursionScentBait] %s is being eaten by %s; gone in %.1fs"),
			*GetNameSafe(this), *GetNameSafe(Eater), EatSeconds);
	}
}

void AExcursionScentBait::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// DeltaTime is dilated: zero while a capture freezes the bait.
	if (!Life.Advance(DeltaTime))
	{
		return;
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionScentBait] %s is gone (%s)"),
		*GetNameSafe(this), Life.IsBeingEaten() ? TEXT("eaten") : TEXT("nobody came"));

	Destroy();
}
