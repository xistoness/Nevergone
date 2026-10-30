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
//
// A volume referenced by an AExcursionDevice masks only while the device is
// on, and never masks the device's own noise (its turn-on noise). A volume
// no device references is always active.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API AExcursionNoiseMaskVolume : public AVolume
{
	GENERATED_BODY()

public:

	AExcursionNoiseMaskVolume();

	float GetHearingMultiplier() const { return HearingMultiplier; }

	void SetMaskActive(bool bActive);

	// The device that switches this volume, or null.
	const AActor* GetDevice() const { return Device.Get(); }

	void SetDevice(const AActor* InDevice) { Device = InDevice; }

	// True when this volume masks a noise that came out of NoiseSource (null
	// when the noise had none).
	bool MasksNoiseFrom(const AActor* NoiseSource) const;

	// The rule behind MasksNoiseFrom, kept pure so it can be tested: an
	// inactive volume masks nothing, and an active one masks everything but
	// the noise of the device that switches it.
	static bool IsMaskApplied(bool bActive, const UObject* InDevice, const UObject* NoiseSource);

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Scales enemy hearing range. 1 has no effect, 0 deafens completely.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HearingMultiplier = 0.5f;

private:

	// Switched by the device; never saved (the excursion never saves halfway).
	bool bMaskActive = true;

	TWeakObjectPtr<const AActor> Device;
};
