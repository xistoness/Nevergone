// Copyright Xyzto Works


#include "World/ExcursionNoiseSubsystem.h"

#include "Nevergone.h"
#include "Actors/ExcursionNoiseMaskVolume.h"
#include "Audio/AudioSubsystem.h"
#include "Data/NevergonePhysicalMaterial.h"
#include "Data/NoiseDefinition.h"

#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Perception/AISense_Hearing.h"

namespace ExcursionNoise
{
	static TAutoConsoleVariable<bool> CVarDebugNoise(
		TEXT("Nevergone.Noise.Debug"),
		false,
		TEXT("Draws every emitted noise and every enemy hearing decision (green heard, red filtered out)."));

	// How long debug shapes stay on screen. Long enough to read after a footstep.
	static constexpr float DebugDrawSeconds = 2.0f;
}

UExcursionNoiseSubsystem* UExcursionNoiseSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;

	return World ? World->GetSubsystem<UExcursionNoiseSubsystem>() : nullptr;
}

bool UExcursionNoiseSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UExcursionNoiseSubsystem::IsDebugEnabled()
{
	return ExcursionNoise::CVarDebugNoise.GetValueOnGameThread();
}

bool UExcursionNoiseSubsystem::IsHeardAsPlaceNoise(FName StimulusTag, bool bMadeByListener)
{
	// The engine does not filter the listener's own noises: a screamer would
	// otherwise answer its own scream.
	if (bMadeByListener || StimulusTag.IsNone())
	{
		return false;
	}

	// Noises that point at a place. Knocking, breaking a door and devices
	// join this list in their own tasks.
	const FName PlaceNoiseTags[] =
	{
		TAG_Noise_Scream.GetTag().GetTagName(),
	};

	for (const FName& PlaceNoiseTag : PlaceNoiseTags)
	{
		if (StimulusTag == PlaceNoiseTag)
		{
			return true;
		}
	}

	return false;
}

void UExcursionNoiseSubsystem::EmitNoise(const UNoiseDefinition* Definition, const FVector& Location,
	AActor* Instigator, const UPhysicalMaterial* Surface)
{
	if (!Definition)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionNoiseSubsystem] %s emitted a noise with no definition; skipped"),
			*GetNameSafe(Instigator));

		return;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	// --- Surface ---

	const EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Surface);

	const UNevergonePhysicalMaterial* NevergoneSurface = Cast<UNevergonePhysicalMaterial>(Surface);

	const float SurfaceMultiplier = NevergoneSurface ? NevergoneSurface->NoiseLoudnessMultiplier : 1.0f;

	// --- Audio: what the player hears ---

	if (USoundBase* Sound = Definition->ResolveSound(SurfaceType))
	{
		UGameInstance* GameInstance = World->GetGameInstance();

		if (UAudioSubsystem* Audio = GameInstance ? GameInstance->GetSubsystem<UAudioSubsystem>() : nullptr)
		{
			float VolumeMultiplier = 1.0f;
			float PitchMultiplier = 1.0f;

			Definition->RollAudioVariation(VolumeMultiplier, PitchMultiplier);

			Audio->PlaySFXAtLocation(Sound, Location, VolumeMultiplier, PitchMultiplier);
		}
	}

	// --- AI: what enemies hear ---

	const float Loudness = Definition->Loudness * SurfaceMultiplier;

	if (Loudness > 0.0f)
	{
		if (Instigator)
		{
			UAISense_Hearing::ReportNoiseEvent(World, Location, Loudness, Instigator,
				Definition->MaxRange, Definition->NoiseTag.GetTagName());
		}
		else
		{
			UE_LOG(LogNevergone, Warning,
				TEXT("[ExcursionNoiseSubsystem] %s has loudness but no instigator; enemies will not hear it"),
				*GetNameSafe(Definition));
		}
	}

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExcursionNoiseSubsystem] %s emitted %s at %s (surface %d, loudness %.2f)"),
		*GetNameSafe(Instigator), *GetNameSafe(Definition), *Location.ToCompactString(),
		static_cast<int32>(SurfaceType), Loudness);

#if ENABLE_DRAW_DEBUG
	if (IsDebugEnabled())
	{
		const FColor Color = Loudness > 0.0f ? FColor::Yellow : FColor::Silver;

		DrawDebugSphere(World, Location, 20.0f, 8, Color, false, ExcursionNoise::DebugDrawSeconds);

		DrawDebugString(World, Location + FVector(0.0f, 0.0f, 40.0f),
			FString::Printf(TEXT("%s x%.2f"), *Definition->NoiseTag.ToString(), Loudness),
			nullptr, Color, ExcursionNoise::DebugDrawSeconds);
	}
#endif
}

void UExcursionNoiseSubsystem::RegisterMaskVolume(AExcursionNoiseMaskVolume* Volume)
{
	if (Volume)
	{
		MaskVolumes.AddUnique(Volume);
	}
}

void UExcursionNoiseSubsystem::UnregisterMaskVolume(AExcursionNoiseMaskVolume* Volume)
{
	MaskVolumes.Remove(Volume);
}

float UExcursionNoiseSubsystem::GetHearingMaskAt(const FVector& Location) const
{
	float Multiplier = 1.0f;

	for (const TWeakObjectPtr<AExcursionNoiseMaskVolume>& Volume : MaskVolumes)
	{
		if (Volume.IsValid() && Volume->EncompassesPoint(Location))
		{
			Multiplier = FMath::Min(Multiplier, Volume->GetHearingMultiplier());
		}
	}

	return Multiplier;
}
