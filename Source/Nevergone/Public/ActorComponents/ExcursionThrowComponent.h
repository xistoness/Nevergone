// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExcursionThrowComponent.generated.h"

class ACharacter;
class UAnimMontage;
class UInstancedStaticMeshComponent;
class UItemDefinition;
class UStaticMesh;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// UExcursionThrowComponent
//
// Throwing distractions during an excursion. Holding the input aims: the arc
// the object will follow is predicted every frame and drawn as a row of
// markers ending on a landing marker. Releasing throws the first item tagged
// Item.Distraction in the inventory, which leaves the inventory at that
// moment. It flies as an AExcursionThrowable and lands as the item's own
// pickup, so picking it up again gives it back.
//
// Driven by UExplorationModeComponent (BeginAim / ReleaseAim / CancelAim), so
// input keeps reaching the survivor only through IExplorationInputReceiver.
// While aiming the survivor turns with the camera and can still move.
//
// The arc is predicted against the object types the thrown object collides
// with, ignoring pawns, so where the landing marker sits is where it lands.
// The marker components are created on the first aim, not on every
// ACharacterBase that carries this component.
// ---------------------------------------------------------------------------
UCLASS(ClassGroup = (Excursion), meta = (BlueprintSpawnableComponent))
class NEVERGONE_API UExcursionThrowComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UExcursionThrowComponent();

	// Starts aiming. Tells the player when there is nothing to throw.
	void BeginAim();

	// Throws along the arc being shown. Does nothing when not aiming.
	void ReleaseAim();

	// Stops aiming without throwing (a screen opened, the mode ended).
	void CancelAim();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Excursion|Throw")
	bool IsAiming() const { return bAiming; }

protected:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- Throw ---

	UPROPERTY(EditDefaultsOnly, Category = "Throw", meta = (ClampMin = "0.0"))
	float LaunchSpeed = 1200.0f;

	// Added to the camera pitch, so a level camera still lobs the object
	// instead of throwing it straight at the floor ahead.
	UPROPERTY(EditDefaultsOnly, Category = "Throw", meta = (Units = "Degrees"))
	float AimPitchBiasDegrees = 10.0f;

	// Socket the object leaves from. Without it, FallbackThrowOffset is used.
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	FName ThrowSocketName = TEXT("hand_r");

	// Launch point relative to the actor when the socket does not exist.
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	FVector FallbackThrowOffset = FVector(30.0f, 0.0f, 50.0f);

	// Optional. Played on release; the object is spawned on the same frame.
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	TObjectPtr<UAnimMontage> ThrowMontage = nullptr;

	// --- Arc ---

	UPROPERTY(EditDefaultsOnly, Category = "Throw|Arc", meta = (ClampMin = "0.1", Units = "Seconds"))
	float MaxPredictionSeconds = 3.0f;

	// Drawn at every predicted point of the arc.
	UPROPERTY(EditDefaultsOnly, Category = "Throw|Arc")
	TObjectPtr<UStaticMesh> ArcMarkerMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Throw|Arc")
	FVector ArcMarkerScale = FVector(0.05f);

	// Drawn where the arc hits, lying on the surface.
	UPROPERTY(EditDefaultsOnly, Category = "Throw|Arc")
	TObjectPtr<UStaticMesh> LandingMarkerMesh = nullptr;

private:

	// First item in the inventory tagged Item.Distraction that has a pickup
	// to fly as and land as.
	UItemDefinition* FindDistraction() const;

	bool ComputeLaunch(FVector& OutStart, FVector& OutVelocity) const;

	void UpdateArc();

	void StopAiming();

	void EnsureArcComponents();

	void HideArc();

	// While aiming the survivor faces the camera instead of its movement.
	void SetAimRotation(bool bAim);

	void ShowPrompt(const FText& Message) const;

	ACharacter* GetCharacter() const;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ArcMarkers = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LandingMarker = nullptr;

	bool bAiming = false;

	bool bSavedOrientRotationToMovement = true;

	bool bSavedUseControllerDesiredRotation = false;
};
