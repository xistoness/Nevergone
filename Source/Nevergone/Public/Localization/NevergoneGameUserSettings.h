// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Audio/AudioCategory.h"
#include "GameFramework/GameUserSettings.h"
#include "NevergoneGameUserSettings.generated.h"

/**
 * UNevergoneGameUserSettings
 *
 * Project-specific extension of UGameUserSettings. Stores player-wide
 * preferences that are NOT tied to a save slot:
 *  - PreferredCulture (UI language)
 *  - Per-category audio volumes (Master / Music / SFX / UI)
 *
 * The base class already manages graphics / audio / input options via
 * GameUserSettings.ini (Saved/Config/<Platform>/). Our UPROPERTY(Config)
 * fields piggyback on that same file with zero extra plumbing.
 *
 * IMPORTANT — engine-side registration:
 *   For the engine to actually instantiate THIS class instead of the base
 *   UGameUserSettings, DefaultEngine.ini must contain:
 *
 *     [/Script/Engine.Engine]
 *     GameUserSettingsClassName=/Script/Nevergone.NevergoneGameUserSettings
 *
 *   Without that line, GetGameUserSettings() returns a base UGameUserSettings
 *   and the cast in GetNevergoneGameUserSettings() returns nullptr.
 */
UCLASS(Config = GameUserSettings, BlueprintType)
class NEVERGONE_API UNevergoneGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:

	// -----------------------------------------------------------------------
	// Static accessor
	// -----------------------------------------------------------------------

	/** Returns the active settings instance, or nullptr if engine registration is missing. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nevergone|Settings")
	static UNevergoneGameUserSettings* GetNevergoneGameUserSettings();

	// -----------------------------------------------------------------------
	// Localization preference
	// -----------------------------------------------------------------------

	/**
	 * Persisted ICU/Unreal culture code for the player's UI language
	 * (e.g. "en", "pt-BR"). Empty string = "not yet set" — the
	 * LocalizationSubsystem then falls back to OS-detected language on first launch.
	 */
	UPROPERTY(Config)
	FString PreferredCulture;

	/** Returns the currently persisted culture code (may be empty on first launch). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nevergone|Settings|Localization")
	const FString& GetPreferredCulture() const { return PreferredCulture; }

	/**
	 * Updates the persisted culture code and writes GameUserSettings.ini to disk.
	 * Does NOT call FInternationalization — that is the LocalizationSubsystem's
	 * responsibility (which calls this method from inside SetCurrentCulture).
	 */
	UFUNCTION(BlueprintCallable, Category = "Nevergone|Settings|Localization")
	void SetPreferredCulture(const FString& InCulture);

	// -----------------------------------------------------------------------
	// Audio preferences
	//
	// All values are normalized [0..1]. Defaults are 1.0 (max) so the game is
	// audible the first time it boots, before the player ever opens Settings.
	// AudioSubsystem applies these values via SetCategoryVolume on Init and
	// whenever the player adjusts a slider.
	// -----------------------------------------------------------------------

	UPROPERTY(Config)
	float MasterVolume = 1.0f;

	UPROPERTY(Config)
	float MusicVolume = 1.0f;

	UPROPERTY(Config)
	float SFXVolume = 1.0f;

	UPROPERTY(Config)
	float UIVolume = 1.0f;

	/** Returns the persisted volume for a category, in the [0..1] range. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nevergone|Settings|Audio")
	float GetCategoryVolume(ENevergoneAudioCategory Category) const;

	/**
	 * Updates the persisted volume for a category and writes
	 * GameUserSettings.ini to disk. Caller is responsible for also
	 * applying the volume to the running mix (AudioSubsystem does this).
	 */
	UFUNCTION(BlueprintCallable, Category = "Nevergone|Settings|Audio")
	void SetCategoryVolume(ENevergoneAudioCategory Category, float NewVolume);

	// -----------------------------------------------------------------------
	// Gameplay preferences
	// -----------------------------------------------------------------------

	/**
	 * True: pressing crouch toggles it. False: crouch lasts while the button
	 * is held. Toggle is the default, and the fallback when this class is not
	 * registered in DefaultEngine.ini.
	 */
	UPROPERTY(Config)
	bool bToggleCrouch = true;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nevergone|Settings|Gameplay")
	bool IsCrouchToggle() const { return bToggleCrouch; }

	/** Updates the crouch mode and writes GameUserSettings.ini to disk. */
	UFUNCTION(BlueprintCallable, Category = "Nevergone|Settings|Gameplay")
	void SetCrouchToggle(bool bNewToggleCrouch);
};
