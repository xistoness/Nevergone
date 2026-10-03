// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "NevergonePhysicalMaterial.generated.h"

// ---------------------------------------------------------------------------
// UNevergonePhysicalMaterial
//
// Physical material with the gameplay data the project reads from surfaces.
// The engine's SurfaceType picks which sound variant plays; this class adds
// how the surface changes what enemies hear.
//
// Assign it to the Phys Material slot of floor materials. Surfaces using a
// plain UPhysicalMaterial behave as multiplier 1.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UNevergonePhysicalMaterial : public UPhysicalMaterial
{
	GENERATED_BODY()

public:

	// Multiplies the AI loudness of every noise made on this surface.
	// Above 1 amplifies (metal grate), below 1 dampens (carpet).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0"))
	float NoiseLoudnessMultiplier = 1.0f;
};
