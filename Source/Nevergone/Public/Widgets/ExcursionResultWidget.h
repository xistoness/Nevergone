// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/ExcursionTypes.h"
#include "ExcursionResultWidget.generated.h"

class UButton;
class UTextBlock;

// ---------------------------------------------------------------------------
// UExcursionResultWidget
//
// End-of-excursion screen, shown by AExplorationPlayerController when
// UExcursionSubsystem broadcasts the result.
//
// Displays:
//   - Header: back from the excursion, or the survivor did not come back
//   - Survivor condition (unharmed, infected, dead) and safeguards used
//   - Rescued survivors
//   - Resources found (or lost, when the survivor died)
//   - Items spent
//
// Continue fires OnContinue; the controller restarts the excursion. Layout,
// styling and animations live in the Blueprint subclass; logic stays here.
// ---------------------------------------------------------------------------
UCLASS(Abstract)
class NEVERGONE_API UExcursionResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void ShowResult(const FExcursionResult& Result);

	// Keyboard and gamepad confirm, routed by the controller. Same as
	// clicking Continue.
	void Confirm();

	DECLARE_MULTICAST_DELEGATE(FOnContinue);
	FOnContinue OnContinue;

protected:

	virtual void NativeOnInitialized() override;

	// -----------------------------------------------------------------------
	// Named UMG widgets — bind in the Blueprint designer
	// -----------------------------------------------------------------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ConditionText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SurvivorsText;

	// "Resources found" or "Resources lost", depending on how it ended.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemsFoundLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemsFoundText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemsSpentText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ContinueButton;

	// Presentation only (entry animation, sound).
	UFUNCTION(BlueprintImplementableEvent, Category = "Excursion Result")
	void OnResultShown(bool bSurvived);

private:

	UFUNCTION()
	void HandleContinueClicked();

	static FText DescribeItems(const TArray<FExcursionItemCount>& Items);

	static FText JoinLines(const TArray<FText>& Lines);

	// Continue restarts the level; a second press before it loads must not
	// restart it again.
	bool bContinued = false;
};
