// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "NevergonePlayerController.h"
#include "MenuController.generated.h"

class USoundBase;

/**
 * Generic controller for all menu screens (main menu, pause, game over, etc.).
 *
 * Responsibilities:
 *   - Starts the configured music track on BeginPlay
 *   - Disables gameplay input, enables UI-only input
 *
 * Usage:
 *   Create a Blueprint child of AMenuController for each menu screen.
 *   Assign MenuMusic in the Blueprint's Details panel.
 *   If a specific menu needs extra behavior, subclass and override BeginPlay.
 */
UCLASS()
class NEVERGONE_API AMenuController : public ANevergonePlayerController
{
	GENERATED_BODY()

protected:

	virtual void BeginPlay() override;

	/**
	 * Music played while this menu is active.
	 * Assign in each Blueprint child — e.g., BP_MainMenuController,
	 * BP_PauseMenuController, etc.
	 * Leave null for silence.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> MenuMusic;
};