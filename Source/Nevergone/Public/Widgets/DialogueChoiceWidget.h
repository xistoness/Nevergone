// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DialogueChoiceWidget.generated.h"

UCLASS()
class NEVERGONE_API UDialogueChoiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void SetupChoice(int32 InIndex, const FText& InLabel, bool bInIsLocked, const FText& InLockedTooltip);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void OnClicked();

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsChoiceEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	int32 GetChoiceIndex() const { return ChoiceIndex; }

protected:

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	int32 ChoiceIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bIsLocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText LockedTooltip;
};