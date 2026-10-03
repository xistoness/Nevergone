// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "AudioCategory.generated.h"

/**
 * Logical audio mixing categories exposed in the Settings UI.
 *
 * Each category is mapped to a concrete USoundClass asset on UMyGameInstance
 * (CategorySoundClasses). The mapping is propagated to the AudioSubsystem at
 * Init time, which is the only place that issues actual SoundMix volume changes.
 *
 * Master is intentionally a real category — it is mixed via the same mechanism
 * as the others (its USoundClass should be the parent of all the others).
 */
UENUM(BlueprintType)
enum class ENevergoneAudioCategory : uint8
{
    Master      UMETA(DisplayName = "Master"),
    Music       UMETA(DisplayName = "Music"),
    SFX         UMETA(DisplayName = "SFX"),
    UI          UMETA(DisplayName = "UI"),
};
