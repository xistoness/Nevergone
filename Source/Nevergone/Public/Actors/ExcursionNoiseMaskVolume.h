// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "ExcursionNoiseMaskVolume.generated.h"

// ---------------------------------------------------------------------------
// AExcursionNoiseMaskVolume
//
// An area loud enough to cover other noises: rain on an open street, a
// running generator, a broken pipe. Enemies hear less when either they or
// the noise are inside it; the lowest multiplier wins where volumes overlap.
//
// Gameplay only. The ambient sound that justifies the mask (the generator
// hum, the rain) is a separate ambient sound actor placed in the level.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API AExcursionNoiseMaskVolume : public AVolume
{
	GENERATED_BODY()

public:

	AExcursionNoiseMaskVolume();

	float GetHearingMultiplier() const { return HearingMultiplier; }

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Scales enemy hearing range. 1 has no effect, 0 deafens completely.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HearingMultiplier = 0.5f;
};
