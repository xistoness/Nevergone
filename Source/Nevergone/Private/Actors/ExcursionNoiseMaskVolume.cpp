// Copyright Xyzto Works


#include "Actors/ExcursionNoiseMaskVolume.h"

#include "Nevergone.h"
#include "World/ExcursionNoiseSubsystem.h"

#include "Components/BrushComponent.h"

AExcursionNoiseMaskVolume::AExcursionNoiseMaskVolume()
{
	// EncompassesPoint queries the brush body, so it needs query collision.
	// Every channel is ignored: the volume must never block movement, traces,
	// or the noise occlusion trace.
	if (UBrushComponent* BrushComp = GetBrushComponent())
	{
		BrushComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BrushComp->SetCollisionResponseToAllChannels(ECR_Ignore);
		BrushComp->SetGenerateOverlapEvents(false);
	}
}

void AExcursionNoiseMaskVolume::BeginPlay()
{
	Super::BeginPlay();

	if (UExcursionNoiseSubsystem* Noise = UExcursionNoiseSubsystem::Get(this))
	{
		Noise->RegisterMaskVolume(this);

		UE_LOG(LogNevergone, Log,
			TEXT("[ExcursionNoiseMaskVolume] %s registered with hearing multiplier %.2f"),
			*GetNameSafe(this), HearingMultiplier);
	}
}

void AExcursionNoiseMaskVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UExcursionNoiseSubsystem* Noise = UExcursionNoiseSubsystem::Get(this))
	{
		Noise->UnregisterMaskVolume(this);
	}

	Super::EndPlay(EndPlayReason);
}
