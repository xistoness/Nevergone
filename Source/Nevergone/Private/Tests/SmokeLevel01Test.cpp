// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Tests/SmokeSaveSlot.h"

#if WITH_DEV_AUTOMATION_TESTS

// Opens the excursion test level and lets it run. Any Error or ensure logged
// meanwhile fails the test (the automation framework's default), so the
// test body only has to load the map and wait.
// Meant to run with -game: in the editor the map would load without playing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneSmokeLevel01Test, "Nevergone.Smoke.Level01",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneSmokeLevel01Test::RunTest(const FString& Parameters)
{
	static const TCHAR* MapName = TEXT("/Game/Blueprints/Level/LevelPrototype/Level_01/Level_01");

	if (!AutomationOpenMap(MapName))
	{
		AddError(FString::Printf(TEXT("Could not open %s"), MapName));
		return false;
	}

	// Like the boot smoke, this one never touches a player slot.
	ADD_LATENT_AUTOMATION_COMMAND(NevergoneSmokeSave::FUseTestSlotCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(10.0f));
	ADD_LATENT_AUTOMATION_COMMAND(NevergoneSmokeSave::FCleanTestSlotCommand());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
