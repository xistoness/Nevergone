// Copyright Xyzto Works

#include "Actors/ExcursionThrowable.h"

#include "Nevergone.h"
#include "Data/ItemDefinition.h"
#include "Data/NoiseDefinition.h"
#include "EngineTools/CollisionChannels.h"
#include "Level/WorldItemActor.h"
#include "World/ExcursionNoiseSubsystem.h"
#include "World/InventorySubsystem.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

AExcursionThrowable::AExcursionThrowable()
{
	PrimaryActorTick.bCanEverTick = false;

	// The projectile sweeps this sphere, so it must be the root.
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(FlightRadius);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(NevergoneCollision::NoiseOcclusion, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(NevergoneCollision::Interactable, ECR_Ignore);
	SetRootComponent(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Starts inactive: nothing flies until Launch.
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->bAutoActivate = false;
	Movement->bShouldBounce = true;
	Movement->Bounciness = 0.3f;
	Movement->Friction = 0.6f;
	Movement->bRotationFollowsVelocity = false;
}

void AExcursionThrowable::Launch(const FVector& Velocity, UItemDefinition* InItem, AActor* InThrower)
{
	Item = InItem;
	Thrower = InThrower;

	CopyPickupLooks();

	Movement->OnProjectileBounce.AddDynamic(this, &AExcursionThrowable::HandleBounce);
	Movement->OnProjectileStop.AddDynamic(this, &AExcursionThrowable::HandleStop);

	Movement->Velocity = Velocity;
	Movement->Activate();
}

void AExcursionThrowable::CopyPickupLooks()
{
	UClass* PickupClass = Item ? Item->WorldActorClass.LoadSynchronous() : nullptr;

	const AWorldItemActor* PickupDefaults = PickupClass ? PickupClass->GetDefaultObject<AWorldItemActor>() : nullptr;

	const UStaticMeshComponent* PickupMesh = PickupDefaults ? PickupDefaults->GetPickupMesh() : nullptr;

	if (!PickupMesh || !PickupMesh->GetStaticMesh())
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionThrowable] %s: %s has no WorldActorClass with a PickupMesh; it flies invisible"),
			*GetNameSafe(this), *GetNameSafe(Item));

		return;
	}

	// Same mesh and same placement relative to the root as the pickup, so
	// the swap on landing does not show.
	Mesh->SetStaticMesh(PickupMesh->GetStaticMesh());
	Mesh->SetRelativeTransform(PickupMesh->GetRelativeTransform());

	for (int32 Index = 0; Index < PickupMesh->GetNumMaterials(); ++Index)
	{
		Mesh->SetMaterial(Index, PickupMesh->GetMaterial(Index));
	}
}

// ---------------------------------------------------------------------------
// Flight
// ---------------------------------------------------------------------------

void AExcursionThrowable::HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	MakeImpactNoise(ImpactResult);
}

void AExcursionThrowable::HandleStop(const FHitResult& ImpactResult)
{
	// Stopping on the very first hit fires no bounce, so the noise may still
	// be owed here.
	MakeImpactNoise(ImpactResult);

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;

	UInventorySubsystem* Inventory = GameInstance ? GameInstance->GetSubsystem<UInventorySubsystem>() : nullptr;

	const AWorldItemActor* Pickup = Inventory
		? Inventory->SpawnWorldItem(Item, GetActorLocation(), GetActorRotation())
		: nullptr;

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionThrowable] %s came to rest; handed over to pickup %s"),
		*GetNameSafe(this), *GetNameSafe(Pickup));

	Destroy();
}

void AExcursionThrowable::MakeImpactNoise(const FHitResult& Hit)
{
	if (bMadeNoise)
	{
		return;
	}

	bMadeNoise = true;

	const UNoiseDefinition* ImpactNoise = Item ? Item->ImpactNoise.Get() : nullptr;

	if (!ImpactNoise)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionThrowable] %s has no ImpactNoise; the throw distracts nobody"),
			*GetNameSafe(Item));

		return;
	}

	UExcursionNoiseSubsystem* Noise = UExcursionNoiseSubsystem::Get(this);

	if (!Noise)
	{
		return;
	}

	// Attributed to the thrower: hearing only accepts survivor noises, and
	// an enemy taking the impact for the survivor is the point.
	Noise->EmitNoise(ImpactNoise, Hit.ImpactPoint, Thrower.Get(), FindSurface(Hit));

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionThrowable] %s hit %s at %s"),
		*GetNameSafe(Item), *GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToCompactString());
}

const UPhysicalMaterial* AExcursionThrowable::FindSurface(const FHitResult& Hit) const
{
	const UWorld* World = GetWorld();

	if (!World)
	{
		return nullptr;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExcursionThrowableSurface), false, this);
	Params.bReturnPhysicalMaterial = true;

	const FVector Start = Hit.ImpactPoint + Hit.ImpactNormal * 10.0f;
	const FVector End = Hit.ImpactPoint - Hit.ImpactNormal * 10.0f;

	FHitResult SurfaceHit;

	if (World->LineTraceSingleByChannel(SurfaceHit, Start, End, ECC_Visibility, Params))
	{
		return SurfaceHit.PhysMaterial.Get();
	}

	return nullptr;
}
