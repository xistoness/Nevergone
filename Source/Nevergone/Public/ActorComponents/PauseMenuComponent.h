// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PauseMenuComponent.generated.h"

class UPauseMenuWidget;
class UInputAction;

// Fired when the pause menu opens/closes so the owning controller can
// update its CurrentInputMode accordingly.
DECLARE_MULTICAST_DELEGATE(FOnPauseOpened);
DECLARE_MULTICAST_DELEGATE(FOnPauseClosed);

/**
 * UPauseMenuComponent
 *
 * Attached to any PlayerController that needs pause menu support.
 * Handles toggle logic, widget lifecycle, and input mode signalling.
 *
 * Input mode is NOT managed here — the owning controller subscribes to
 * OnPauseOpened / OnPauseClosed and applies its own mode (PauseMenu / restore).
 * This keeps the component decoupled from EExplorationInputMode.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class NEVERGONE_API UPauseMenuComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UPauseMenuComponent();

	void BindPauseInput(class UEnhancedInputComponent* EIC, UInputAction* PauseAction);
	void CreatePauseWidget();
	void TogglePause();

	bool IsPaused() const { return bIsPaused; }

	UFUNCTION(BlueprintCallable, Category = "Pause")
	void Resume();

	// Navigation forwarding — called by the owning controller's Handle* methods.
	void NavigateUp();
	void NavigateDown();
	void ConfirmSelection();
	bool CancelSelection(); // returns false when controller should close

	UPROPERTY(EditDefaultsOnly, Category = "Pause|UI")
	TSubclassOf<UPauseMenuWidget> PauseMenuWidgetClass;

	FOnPauseOpened OnPauseOpened;
	FOnPauseClosed OnPauseClosed;

private:

	void OpenPause();
	void ClosePause();

	UPROPERTY()
	UPauseMenuWidget* PauseWidget = nullptr;

	bool bIsPaused = false;
};