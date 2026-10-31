// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExcursionThrowable.generated.h"

class UItemDefinition;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// AExcursionThrowable
//
// A thrown distraction while it flies, and nothing else. Spawned by
// UExcursionThrowComponent; no Blueprint per item: everything item-specific
// comes from the UItemDefinition.
//
//   Looks     The mesh of the item's WorldActorClass pickup, copied from its
//             class default object, so what flies looks like what is
//             picked up.
//   Noise     The item's ImpactNoise on the first impact, at the impact
//             point and attributed to the thrower, so enemies take it for
//             the survivor. Later bounces are silent.
//   Landing   Once it stops it spawns the item's pickup in its place and
//             destroys itself. Picking up is AWorldItemActor's job. An item
//             tagged Item.ScentBait lands as an AExcursionScentBait instead,
//             which cannot be picked up.
//
// Flight and pickup need opposite collision: a small body that blocks the
// world to bounce, against a large overlap-only sphere that is easy to aim
// at. That is why they are two actors.
//
// The flight body ignores pawns (never hits the thrower or an enemy),
// visibility (never blocks enemy sight) and noise occlusion.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API AExcursionThrowable : public AActor
{
	GENERATED_BODY()

public:

	// Radius of the flight body. UExcursionThrowComponent predicts the arc
	// with the same value, so the landing marker matches the landing.
	static constexpr float FlightRadius = 5.0f;

	AExcursionThrowable();

	void Launch(const FVector& Velocity, UItemDefinition* InItem, AActor* InThrower);

	// Gives Target the mesh, placement and materials of the item's pickup
	// (its WorldActorClass), copied from the class default object. False
	// when the item has no pickup mesh. LogContext is for the log only.
	static bool CopyPickupLooks(const UItemDefinition* InItem, UStaticMeshComponent* Target, const AActor* LogContext);

protected:

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:

	UFUNCTION()
	void HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);

	// Leaves the bait the item lands as. False when it could not be spawned.
	bool SpawnScentBait();

	void MakeImpactNoise(const FHitResult& Hit);

	// The projectile sweep does not return physical materials, so the
	// surface under the impact is traced separately.
	const UPhysicalMaterial* FindSurface(const FHitResult& Hit) const;

	UPROPERTY()
	TObjectPtr<UItemDefinition> Item = nullptr;

	TWeakObjectPtr<AActor> Thrower;

	bool bMadeNoise = false;
};
