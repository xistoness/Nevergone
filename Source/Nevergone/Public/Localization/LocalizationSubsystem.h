// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LocalizationSubsystem.generated.h"

/** Broadcast right after the active culture successfully changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCultureChanged, const FString&, NewCultureCode);

/**
 * ULocalizationSubsystem
 *
 * Single execution point for all language-switching logic in Nevergone.
 *
 * Responsibilities:
 *   - On startup, applies the player's persisted culture (or detects from OS).
 *   - Exposes the list of supported cultures (configured on UMyGameInstance).
 *   - Lets the UI switch culture at runtime via SetCurrentCulture().
 *   - Persists the choice into UNevergoneGameUserSettings.
 *   - Broadcasts OnCultureChanged so widgets that need extra refresh
 *     (e.g. dropdowns showing per-culture labels) can listen.
 *
 * Automatic UTextBlock refresh — important to internalize:
 *   Any FText built via NSLOCTEXT / LOCTEXT / FText::Format(LOCTEXT(...))
 *   refreshes automatically when FInternationalization::SetCurrentCulture
 *   is called, because UTextBlock listens to FTextLocalizationManager's
 *   global revision counter.
 *   FText::FromString(...) does NOT participate in that mechanism — those
 *   FTexts have no namespace/key and the engine cannot re-resolve them.
 *   Player-facing strings must therefore always come from LOCTEXT-family
 *   macros, FText::Format on top of those, FText fields in DataAssets, or
 *   UEnum metadata via GetDisplayValueAsText.
 */
UCLASS()
class NEVERGONE_API ULocalizationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	// -----------------------------------------------------------------------
	// Subsystem lifecycle
	// -----------------------------------------------------------------------

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// -----------------------------------------------------------------------
	// Public API
	// -----------------------------------------------------------------------

	/** Returns all culture codes the project ships with, as configured on UMyGameInstance. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nevergone|Localization")
	const TArray<FString>& GetSupportedCultures() const { return SupportedCultures; }

	/** Returns the currently active culture code (e.g. "en", "pt-BR"). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nevergone|Localization")
	FString GetCurrentCulture() const;

	/**
	 * Returns a display name for the given culture code, written in that culture's
	 * own native form ("English", "Português (Brasil)"). Useful for populating a
	 * language dropdown — the label remains readable regardless of which culture
	 * is currently active.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nevergone|Localization")
	FText GetCultureDisplayName(const FString& CultureCode) const;

	/**
	 * Activates the given culture, persists the choice to GameUserSettings.ini,
	 * and broadcasts OnCultureChanged. UTextBlocks bound to LOCTEXT/NSLOCTEXT/Format
	 * texts refresh automatically on the next paint.
	 *
	 * Returns true on success; false if the culture is unsupported or the engine
	 * refuses to switch (very rare — usually means the .locres files are missing
	 * for that target culture).
	 */
	UFUNCTION(BlueprintCallable, Category = "Nevergone|Localization")
	bool SetCurrentCulture(const FString& CultureCode);

	/** Fired right after SetCurrentCulture successfully changes the active culture. */
	UPROPERTY(BlueprintAssignable, Category = "Nevergone|Localization")
	FOnCultureChanged OnCultureChanged;

private:

	// -----------------------------------------------------------------------
	// Private helpers
	// -----------------------------------------------------------------------

	/**
	 * Picks the culture to apply on first run, in priority order:
	 *   1. PreferredCulture from GameUserSettings (if non-empty AND supported)
	 *   2. OS-reported culture (full name, e.g. "pt-BR")
	 *   3. OS-reported culture's two-letter language prefix (e.g. "pt")
	 *   4. FallbackCulture from UMyGameInstance (if supported)
	 *   5. The first entry in SupportedCultures (last resort)
	 */
	FString DetermineInitialCulture() const;

	/** Returns true if CultureCode is in SupportedCultures (case-insensitive). */
	bool IsCultureSupported(const FString& CultureCode) const;

	// -----------------------------------------------------------------------
	// State (snapshotted from UMyGameInstance at Initialize time)
	// -----------------------------------------------------------------------

	/** All cultures the project supports — copied from UMyGameInstance. */
	UPROPERTY()
	TArray<FString> SupportedCultures;

	/** Hard fallback used when nothing else matches — copied from UMyGameInstance. */
	UPROPERTY()
	FString FallbackCulture;
};
