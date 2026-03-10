// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingScreenWidget.generated.h"

/**
 * 
 */
UCLASS()
class NEVERGONE_API ULoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:

	// Called when loading starts
	UFUNCTION(BlueprintCallable, Category="Loading")
	virtual void OnLoadingStarted();

	// Called when loading finishes
	UFUNCTION(BlueprintCallable, Category="Loading")
	virtual void OnLoadingFinished();

	// Optional: update tip text
	UFUNCTION(BlueprintImplementableEvent, Category="Loading")
	void UpdateTip(const FText& NewTip);
};
