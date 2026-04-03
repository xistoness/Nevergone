// Copyright Xyzto Works

#include "Characters/PlayerControllers/MenuController.h"

#include "Audio/AudioSubsystem.h"

void AMenuController::BeginPlay()
{
	Super::BeginPlay();

	// Start menu music — each Blueprint child configures its own MenuMusic asset
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
		{
			Audio->PlayMusic(MenuMusic, EMusicState::MainMenu);
			UE_LOG(LogTemp, Log, TEXT("[MenuController] Music started: %s"), *GetNameSafe(MenuMusic));
		}
	}
}