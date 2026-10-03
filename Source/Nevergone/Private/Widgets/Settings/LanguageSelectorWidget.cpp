// Copyright Xyzto Works

#include "Widgets/Settings/LanguageSelectorWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Localization/LocalizationSubsystem.h"

void ULanguageSelectorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Cache the subsystem once. UGameInstanceSubsystems live for the entire
	// session, so caching here is safe and avoids a lookup per click.
	if (UGameInstance* GI = GetGameInstance())
	{
		LocalizationSubsystem = GI->GetSubsystem<ULocalizationSubsystem>();
	}

	if (!LocalizationSubsystem)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[LanguageSelectorWidget] NativeOnInitialized: LocalizationSubsystem unavailable"));
		return;
	}

	// Wire button clicks. AddDynamic requires UFUNCTION-tagged handlers.
	if (Btn_Prev)
	{
		Btn_Prev->OnClicked.AddDynamic(this, &ULanguageSelectorWidget::HandlePrevClicked);
	}
	if (Btn_Next)
	{
		Btn_Next->OnClicked.AddDynamic(this, &ULanguageSelectorWidget::HandleNextClicked);
	}

	// Reflect external culture switches (e.g. another widget triggering the
	// change, or initial culture applied by the subsystem at startup).
	LocalizationSubsystem->OnCultureChanged.AddDynamic(
		this, &ULanguageSelectorWidget::HandleCultureChanged);

	RefreshDisplay();

	UE_LOG(LogTemp, Log,
		TEXT("[LanguageSelectorWidget] Initialized — current culture '%s'"),
		*LocalizationSubsystem->GetCurrentCulture());
}

void ULanguageSelectorWidget::NativeDestruct()
{
	// Unsubscribe to avoid stale invocations after the widget is removed.
	// Buttons clean up their own bindings when they are destroyed alongside the widget.
	if (LocalizationSubsystem)
	{
		LocalizationSubsystem->OnCultureChanged.RemoveDynamic(
			this, &ULanguageSelectorWidget::HandleCultureChanged);
	}

	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Button handlers
// ---------------------------------------------------------------------------

void ULanguageSelectorWidget::HandlePrevClicked()
{
	CycleLanguage(-1);
}

void ULanguageSelectorWidget::HandleNextClicked()
{
	CycleLanguage(+1);
}

void ULanguageSelectorWidget::CycleLanguage(int32 Direction)
{
	if (!LocalizationSubsystem) { return; }

	const TArray<FString>& Supported = LocalizationSubsystem->GetSupportedCultures();
	if (Supported.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LanguageSelectorWidget] CycleLanguage: SupportedCultures is empty — nothing to do"));
		return;
	}

	const FString Current = LocalizationSubsystem->GetCurrentCulture();

	// Find the index of the current culture, falling back to 0 if not in the list.
	int32 CurrentIndex = Supported.IndexOfByPredicate([&Current](const FString& Code)
	{
		return Code.Equals(Current, ESearchCase::IgnoreCase);
	});
	if (CurrentIndex == INDEX_NONE) { CurrentIndex = 0; }

	// Wrap-around modulo. Adding Num() before % handles negative directions safely.
	const int32 Count    = Supported.Num();
	const int32 NewIndex = ((CurrentIndex + Direction) % Count + Count) % Count;

	const FString& NewCulture = Supported[NewIndex];

	UE_LOG(LogTemp, Log,
		TEXT("[LanguageSelectorWidget] CycleLanguage(%d): '%s' -> '%s'"),
		Direction, *Current, *NewCulture);

	// SetCurrentCulture broadcasts OnCultureChanged, which calls back into
	// HandleCultureChanged below to refresh the label.
	LocalizationSubsystem->SetCurrentCulture(NewCulture);
}

// ---------------------------------------------------------------------------
// Subsystem bridge
// ---------------------------------------------------------------------------

void ULanguageSelectorWidget::HandleCultureChanged(const FString& NewCultureCode)
{
	UE_LOG(LogTemp, Verbose,
		TEXT("[LanguageSelectorWidget] HandleCultureChanged: '%s'"), *NewCultureCode);

	RefreshDisplay();
}

void ULanguageSelectorWidget::RefreshDisplay()
{
	if (!Lbl_Language || !LocalizationSubsystem) { return; }

	const FString CurrentCulture  = LocalizationSubsystem->GetCurrentCulture();
	const FText DisplayName       = LocalizationSubsystem->GetCultureDisplayName(CurrentCulture);

	Lbl_Language->SetText(DisplayName);
}
