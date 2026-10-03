// Copyright Xyzto Works


#include "Data/NoiseDefinition.h"

#include "Sound/SoundBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

USoundBase* UNoiseDefinition::ResolveSound(EPhysicalSurface Surface) const
{
	if (const TObjectPtr<USoundBase>* Found = SurfaceSounds.Find(Surface))
	{
		if (*Found)
		{
			return *Found;
		}
	}

	return DefaultSound;
}

void UNoiseDefinition::RollAudioVariation(float& OutVolumeMultiplier, float& OutPitchMultiplier) const
{
	OutVolumeMultiplier = VolumeVariation > 0.0f
		? FMath::FRandRange(1.0f - VolumeVariation, 1.0f + VolumeVariation)
		: 1.0f;

	// Pitch drives playback rate, so it must never reach zero.
	OutPitchMultiplier = PitchVariation > 0.0f
		? FMath::Max(0.1f, FMath::FRandRange(1.0f - PitchVariation, 1.0f + PitchVariation))
		: 1.0f;
}

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "NoiseDefinition"

EDataValidationResult UNoiseDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (ReportsToAI() && !NoiseTag.IsValid())
	{
		Context.AddWarning(LOCTEXT("MissingTag",
			"Loudness is above zero but NoiseTag is empty. Enemies will hear it, but cannot tell what it was."));
	}

	if (!DefaultSound && SurfaceSounds.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("NoSound",
			"No sound assigned. The player will not hear this noise, even though enemies might."));
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE

#endif
