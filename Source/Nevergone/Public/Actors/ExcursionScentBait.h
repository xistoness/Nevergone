// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/ScentBaitRules.h"
#include "ExcursionScentBait.generated.h"

class UItemDefinition;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// AExcursionScentBait
//
// A thrown scent bait lying where it landed. Spawned by AExcursionThrowable
// for an item tagged Item.ScentBait; no Blueprint per item: the look (the
// mesh of the item's WorldActorClass pickup) and the numbers (ScentRadius,
// ScentEatSeconds, ScentGroundSeconds) come from the UItemDefinition.
//
// It cannot be picked up and blocks nothing. It registers with
// UExcursionScentSubsystem, where the smell poll of every enemy that can
// smell finds it; the enemy's controller walks there and eats it, and tells
// the bait when it starts (NotifyEatingStarted).
//
// Its life (FScentBaitLife): the ground time while nobody eats it, then the
// eating time from the first enemy that starts; it is destroyed when either
// runs out, and the enemies eating it notice it is gone. Counted with its
// own dilated delta time, and a player capture freezes it with the rest of
// the world (UExcursionEncounterSubsystem), so it does not age meanwhile.
// ---------------------------------------------------------------------------
UCLASS(NotBlueprintable)
class NEVERGONE_API AExcursionScentBait : public AActor
{
	GENERATED_BODY()

public:

	AExcursionScentBait();

	// Takes the look and the numbers of the item. Call between a deferred
	// spawn and FinishSpawning.
	void Initialize(const UItemDefinition* InItem);

	// Enemies within this distance of the bait smell it.
	float GetScentRadius() const { return ScentRadius; }

	// An enemy started eating. The first one starts the eating time.
	void NotifyEatingStarted(const AActor* Eater);

	bool IsBeingEaten() const { return Life.IsBeingEaten(); }

	virtual void Tick(float DeltaTime) override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

private:

	UPROPERTY()
	TObjectPtr<const UItemDefinition> Item = nullptr;

	float ScentRadius = 0.0f;

	float EatSeconds = 0.0f;

	FScentBaitLife Life;
};
