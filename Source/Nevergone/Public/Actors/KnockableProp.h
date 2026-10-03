// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnockableProp.generated.h"

class UBoxComponent;
class UNoiseDefinition;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// AKnockableProp
//
// A loose object (bottle, can pile, lamp) that falls over when the survivor
// brushes past it too fast, and makes a noise whenever it is hit hard enough.
//
// Knocking:
//   Physics stays off until the first knock, so props placed on shelves do
//   not settle or drift at level start. While a survivor body (the player's
//   survivor or a recruited follower) is inside BumpTrigger, its horizontal
//   speed is checked every tick: contact alone is not enough, slow movement
//   (crouching) never knocks.
//   After knocking the prop a survivor must leave the trigger and enter it
//   again to knock it a second time, so standing against it does not kick
//   it repeatedly. Each survivor is armed separately.
//
// Noise:
//   Every physics impact is a candidate, filtered by two gates. The impact
//   must be hard enough (MinImpactSpeed), which rules out resting contacts,
//   scraping and the shelf the prop was standing on, and noises cannot
//   follow each other faster than MinNoiseIntervalSeconds, which keeps one
//   fall from sounding like three.
//
// Deliberately not saveable. An excursion is a single session: the game
// saves on entering the building and never during it, so every load starts
// with every prop standing.
// ---------------------------------------------------------------------------
UCLASS(Blueprintable)
class NEVERGONE_API AKnockableProp : public AActor
{
	GENERATED_BODY()

public:

	AKnockableProp();

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void BeginPlay() override;

	// Visual and physical body. Mesh assigned in the Blueprint subclass.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PropMesh;

	// Slightly larger than the mesh, so the survivor's capsule reaches it
	// before being stopped by the mesh itself. Sized per prop in the Blueprint.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BumpTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockable")
	TObjectPtr<UNoiseDefinition> ImpactNoise;

	// Survivor horizontal speed, in cm/s, at or above which contact knocks
	// the prop.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockable", meta = (ClampMin = "0.0"))
	float KnockSpeedThreshold = 400.0f;

	// Velocity change applied on knock, in cm/s. Mass-independent, so a heavy
	// lamp and a light can topple alike.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockable", meta = (ClampMin = "0.0"))
	float KnockVelocity = 150.0f;

	// Impact strength, in cm/s of velocity change, below which a contact is
	// silent. Resting on a shelf, sliding and being pushed stay under it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockable|Noise", meta = (ClampMin = "0.0"))
	float MinImpactSpeed = 150.0f;

	// Minimum time between two noises from this prop, in seconds. A fall
	// lands and bounces; this makes it one noise.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knockable|Noise", meta = (ClampMin = "0.0"))
	float MinNoiseIntervalSeconds = 0.5f;

private:

	UFUNCTION()
	void HandleBumpBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleBumpEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void HandlePropHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// Knocks the prop for the first armed survivor inside the trigger who is
	// fast enough.
	void EvaluateBump();

	void Knock(APawn* ByPawn);

	// Survivors currently inside BumpTrigger, each with whether it can still
	// knock the prop before leaving. The tick runs while this is not empty.
	TMap<TWeakObjectPtr<APawn>, bool> ArmedByOverlappingSurvivor;

	// Survivor who knocked the prop last. Instigator of the impact noise,
	// even if they have walked away by the time it lands.
	TWeakObjectPtr<APawn> KnockedBy;

	bool bPhysicsEnabled = false;

	float LastNoiseTime = -BIG_NUMBER;
};
