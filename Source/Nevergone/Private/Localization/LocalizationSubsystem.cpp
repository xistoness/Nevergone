// Copyright Xyzto Works

#include "Localization/LocalizationSubsystem.h"

#include "GameInstance/MyGameInstance.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Localization/NevergoneGameUserSettings.h"

// ===========================================================================
// Lifecycle
// ===========================================================================

void ULocalizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Snapshot config from UMyGameInstance. Subsystems are initialized inside
	// UGameInstance::Init() — by this point the GI's UPROPERTY defaults
	// (including any BP-class overrides) are already applied.
	if (const UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		SupportedCultures = GI->SupportedCultures;
		FallbackCulture   = GI->FallbackCulture;
	}
	else
	{
		// Hard-coded last-resort defaults — should never trigger in production.
		UE_LOG(LogTemp, Warning,
			TEXT("[LocalizationSubsystem] Initialize: GameInstance is not UMyGameInstance — using hard defaults"));
		SupportedCultures = { TEXT("en") };
		FallbackCulture   = TEXT("en");
	}

	UE_LOG(LogTemp, Log,
		TEXT("[LocalizationSubsystem] Initialize: %d supported culture(s), fallback='%s'"),
		SupportedCultures.Num(), *FallbackCulture);

	// Apply the initial culture. We deliberately go through the public
	// SetCurrentCulture so the same code path persists/broadcasts at startup —
	// this guarantees GameUserSettings.ini gets the resolved value on first launch.
	const FString InitialCulture = DetermineInitialCulture();
	SetCurrentCulture(InitialCulture);
}

void ULocalizationSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("[LocalizationSubsystem] Deinitialize"));
	Super::Deinitialize();
}

// ===========================================================================
// Public API
// ===========================================================================

FString ULocalizationSubsystem::GetCurrentCulture() const
{
	return FInternationalization::Get().GetCurrentCulture()->GetName();
}

FText ULocalizationSubsystem::GetCultureDisplayName(const FString& CultureCode) const
{
	// FInternationalization::GetCulture returns FCulturePtr (TSharedPtr<FCulture>) —
	// nullptr indicates the engine has no metadata for that code. We return the
	// raw code as a fallback so the UI never displays an empty cell.
	if (FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureCode))
	{
		return FText::FromString(Culture->GetNativeName());
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[LocalizationSubsystem] GetCultureDisplayName: unknown culture '%s'"),
		*CultureCode);

	return FText::FromString(CultureCode);
}

bool ULocalizationSubsystem::SetCurrentCulture(const FString& CultureCode)
{
	if (!IsCultureSupported(CultureCode))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LocalizationSubsystem] SetCurrentCulture: '%s' is not in SupportedCultures — request ignored"),
			*CultureCode);
		return false;
	}

	// =====================================================================
	// Two-step culture switch (editor + runtime)
	// =====================================================================
	//
	// Step 1 — SetCurrentLanguageAndLocale
	//   Sets the engine's Language (drives FText resolution) and Locale
	//   (drives number/date formatting) for the runtime/Game channel.
	//   Triggers FTextLocalizationManager's revision counter so all UMG
	//   UTextBlocks re-resolve their LOCTEXT/NSLOCTEXT/Format sources on
	//   the next paint. This is the call that works in PACKAGED builds.
	//
	// Step 2 — EnableGameLocalizationPreview (editor only)
	//   In PIE, FTextLocalizationManager::OnCultureChanged uses
	//   FApp::IsGame() to decide whether to load Game-target .locres files.
	//   In PIE that flag returns false (we're technically inside the editor
	//   process), so the Game .locres are NOT loaded and the running
	//   widgets see no translation — even though the engine internal
	//   "current culture" did flip and the editor itself updates.
	//   EnableGameLocalizationPreview is the official escape hatch:
	//   it forces the manager to load the Game target as if we were a
	//   packaged build. In a packaged build this call is harmless (no-op).
	//
	// Symptom this two-step fixes:
	//   - Without step 2 in PIE: editor toolbars/menus translate, the
	//     PIE game world does not.
	//   - Without step 1 in packaged build: nothing translates (the editor
	//     channel is irrelevant outside the editor).

	// Step 1: runtime/Game channel — works in packaged builds.
	if (!FInternationalization::Get().SetCurrentLanguageAndLocale(CultureCode))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[LocalizationSubsystem] SetCurrentCulture: engine refused to switch to '%s' (missing .locres?)"),
			*CultureCode);
		return false;
	}

	// Step 2: PIE preview — only compiled in editor builds.
	// FTextLocalizationManager is the singleton; EnableGameLocalizationPreview
	// loads the Game target's .locres into the active culture in PIE.
#if WITH_EDITOR
	FTextLocalizationManager::Get().EnableGameLocalizationPreview(CultureCode);
	UE_LOG(LogTemp, Verbose,
		TEXT("[LocalizationSubsystem] EnableGameLocalizationPreview('%s') — PIE-only path"),
		*CultureCode);
#endif

	// Persist player choice. The settings object writes to disk synchronously.
	if (UNevergoneGameUserSettings* Settings = UNevergoneGameUserSettings::GetNevergoneGameUserSettings())
	{
		Settings->SetPreferredCulture(CultureCode);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LocalizationSubsystem] SetCurrentCulture: NevergoneGameUserSettings unavailable — ")
			TEXT("choice NOT persisted (check DefaultEngine.ini GameUserSettingsClassName entry)"));
	}

	UE_LOG(LogTemp, Log,
		TEXT("[LocalizationSubsystem] Culture switched to '%s' — broadcasting OnCultureChanged"),
		*CultureCode);

	OnCultureChanged.Broadcast(CultureCode);
	return true;
}

// ===========================================================================
// Private helpers
// ===========================================================================

FString ULocalizationSubsystem::DetermineInitialCulture() const
{
	// 1. Persisted preference takes priority — respect explicit player choice.
	if (const UNevergoneGameUserSettings* Settings = UNevergoneGameUserSettings::GetNevergoneGameUserSettings())
	{
		const FString& Persisted = Settings->GetPreferredCulture();
		if (!Persisted.IsEmpty() && IsCultureSupported(Persisted))
		{
			UE_LOG(LogTemp, Log,
				TEXT("[LocalizationSubsystem] DetermineInitialCulture: using persisted '%s'"),
				*Persisted);
			return Persisted;
		}
	}

	// 2. OS-reported culture — first-launch default for the player's region.
	//    GetDefaultCulture() returns the OS culture as detected by ICU at startup.
	if (FCulturePtr OSCulture = FInternationalization::Get().GetDefaultCulture())
	{
		const FString OSName = OSCulture->GetName();
		if (IsCultureSupported(OSName))
		{
			UE_LOG(LogTemp, Log,
				TEXT("[LocalizationSubsystem] DetermineInitialCulture: using OS culture '%s'"),
				*OSName);
			return OSName;
		}

		// Try the language-only prefix ("pt-BR" -> "pt") before giving up — the
		// player may have an unsupported regional variant we still recognise.
		const FString OSLanguage = OSCulture->GetTwoLetterISOLanguageName();
		if (IsCultureSupported(OSLanguage))
		{
			UE_LOG(LogTemp, Log,
				TEXT("[LocalizationSubsystem] DetermineInitialCulture: using OS language prefix '%s' (full='%s' unsupported)"),
				*OSLanguage, *OSName);
			return OSLanguage;
		}

		UE_LOG(LogTemp, Log,
			TEXT("[LocalizationSubsystem] DetermineInitialCulture: OS culture '%s' (lang '%s') not supported — falling back"),
			*OSName, *OSLanguage);
	}

	// 3. Configured fallback (almost always English).
	if (IsCultureSupported(FallbackCulture))
	{
		UE_LOG(LogTemp, Log,
			TEXT("[LocalizationSubsystem] DetermineInitialCulture: using configured fallback '%s'"),
			*FallbackCulture);
		return FallbackCulture;
	}

	// 4. Hard last resort: the very first supported culture.
	if (SupportedCultures.Num() > 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LocalizationSubsystem] DetermineInitialCulture: fallback '%s' not in SupportedCultures — using first entry '%s'"),
			*FallbackCulture, *SupportedCultures[0]);
		return SupportedCultures[0];
	}

	// Should never happen — UMyGameInstance defaults guarantee at least one entry.
	UE_LOG(LogTemp, Error,
		TEXT("[LocalizationSubsystem] DetermineInitialCulture: SupportedCultures is empty — defaulting to 'en'"));
	return TEXT("en");
}

bool ULocalizationSubsystem::IsCultureSupported(const FString& CultureCode) const
{
	if (CultureCode.IsEmpty())
	{
		return false;
	}

	return SupportedCultures.ContainsByPredicate([&CultureCode](const FString& Entry)
	{
		return Entry.Equals(CultureCode, ESearchCase::IgnoreCase);
	});
}
