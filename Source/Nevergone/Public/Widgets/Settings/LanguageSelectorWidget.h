// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LanguageSelectorWidget.generated.h"

class UButton;
class UTextBlock;
class ULocalizationSubsystem;

/**
 * ULanguageSelectorWidget
 *
 * Joystick-friendly language picker: ◀  Português (Brasil)  ▶
 *
 * Cycles through ULocalizationSubsystem::GetSupportedCultures(). Applies
 * the new culture immediately on click — UTextBlocks bound to
 * LOCTEXT/NSLOCTEXT/Format text re-resolve themselves automatically through
 * FTextLocalizationManager's revision counter, so the entire UI updates
 * on the next paint with no extra subscription work.
 *
 * Author contract — WBP_LanguageSelector must:
 *   - inherit ULanguageSelectorWidget
 *   - contain a UButton named "Btn_Prev"
 *   - contain a UButton named "Btn_Next"
 *   - contain a UTextBlock named "Lbl_Language"
 */
UCLASS(Abstract)
class NEVERGONE_API ULanguageSelectorWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

protected:

	// -----------------------------------------------------------------------
	// BindWidget — names must match exactly in WBP_LanguageSelector
	// -----------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Btn_Prev;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Btn_Next;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* Lbl_Language;

private:

	// -----------------------------------------------------------------------
	// Button handlers
	// -----------------------------------------------------------------------

	UFUNCTION()
	void HandlePrevClicked();

	UFUNCTION()
	void HandleNextClicked();

	/** Common cycler. Direction = -1 for prev, +1 for next. */
	void CycleLanguage(int32 Direction);

	// -----------------------------------------------------------------------
	// Subsystem bridge
	// -----------------------------------------------------------------------

	/** Listens to ULocalizationSubsystem::OnCultureChanged for external switches. */
	UFUNCTION()
	void HandleCultureChanged(const FString& NewCultureCode);

	/** Re-renders Lbl_Language using the active culture's native name. */
	void RefreshDisplay();

	/** Cached reference to the LocalizationSubsystem. */
	UPROPERTY()
	TObjectPtr<ULocalizationSubsystem> LocalizationSubsystem;
};
