// Copyright Xyzto Works

#include "Localization/NevergoneGameUserSettings.h"

UNevergoneGameUserSettings* UNevergoneGameUserSettings::GetNevergoneGameUserSettings()
{
	// UGameUserSettings::GetGameUserSettings is the engine's singleton accessor.
	// The cast resolves to nullptr when DefaultEngine.ini does not register our subclass —
	// callers must handle that case (LocalizationSubsystem logs a warning and proceeds without persistence).
	return Cast<UNevergoneGameUserSettings>(UGameUserSettings::GetGameUserSettings());
}

void UNevergoneGameUserSettings::SetPreferredCulture(const FString& InCulture)
{
	if (PreferredCulture == InCulture)
	{
		// Nothing to persist — avoid an unnecessary disk write on rapid same-value sets.
		return;
	}

	PreferredCulture = InCulture;

	UE_LOG(LogTemp, Log,
		TEXT("[NevergoneGameUserSettings] PreferredCulture set to '%s' — committing to disk"),
		*PreferredCulture);

	// Synchronous write to GameUserSettings.ini. The file is a few KB at most, and this
	// path is only triggered by explicit user action in the Settings panel.
	SaveSettings();
}

void UNevergoneGameUserSettings::SetCrouchToggle(bool bNewToggleCrouch)
{
	if (bToggleCrouch == bNewToggleCrouch)
	{
		// Nothing to persist — avoid an unnecessary disk write.
		return;
	}

	bToggleCrouch = bNewToggleCrouch;

	UE_LOG(LogTemp, Log,
		TEXT("[NevergoneGameUserSettings] Crouch mode set to %s — committing to disk"),
		bToggleCrouch ? TEXT("toggle") : TEXT("hold"));

	SaveSettings();
}

float UNevergoneGameUserSettings::GetCategoryVolume(ENevergoneAudioCategory Category) const
{
	// A switch is used here (instead of a TMap) to keep this method non-allocating
	// and inline-friendly — it is called every time the Audio tab refreshes.
	switch (Category)
	{
		case ENevergoneAudioCategory::Master: return MasterVolume;
		case ENevergoneAudioCategory::Music:  return MusicVolume;
		case ENevergoneAudioCategory::SFX:    return SFXVolume;
		case ENevergoneAudioCategory::UI:     return UIVolume;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[NevergoneGameUserSettings] GetCategoryVolume: unknown category enum value %d — returning 1.0"),
		static_cast<int32>(Category));
	return 1.0f;
}

void UNevergoneGameUserSettings::SetCategoryVolume(ENevergoneAudioCategory Category, float NewVolume)
{
	const float ClampedVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);

	float* TargetField = nullptr;
	switch (Category)
	{
		case ENevergoneAudioCategory::Master: TargetField = &MasterVolume; break;
		case ENevergoneAudioCategory::Music:  TargetField = &MusicVolume;  break;
		case ENevergoneAudioCategory::SFX:    TargetField = &SFXVolume;    break;
		case ENevergoneAudioCategory::UI:     TargetField = &UIVolume;     break;
	}

	if (!TargetField)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NevergoneGameUserSettings] SetCategoryVolume: unknown category enum value %d — request ignored"),
			static_cast<int32>(Category));
		return;
	}

	if (FMath::IsNearlyEqual(*TargetField, ClampedVolume))
	{
		// Skip same-value disk writes (sliders can fire many tiny updates per drag).
		return;
	}

	*TargetField = ClampedVolume;

	UE_LOG(LogTemp, Log,
		TEXT("[NevergoneGameUserSettings] Volume %d set to %.2f — committing to disk"),
		static_cast<int32>(Category), ClampedVolume);

	SaveSettings();
}
