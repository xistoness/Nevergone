// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "World/ExcursionEncounterSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneExcursionDeathChanceTest, "Nevergone.Excursion.DeathChance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneExcursionDeathChanceTest::RunTest(const FString& Parameters)
{
	// Base chance alone.
	TestEqual(TEXT("Base only"),
		UExcursionEncounterSubsystem::CalculateDeathChance(40.0f, 0, 0, 5.0f, 10.0f), 40.0f);

	// Each point of Strength lowers the chance.
	TestEqual(TEXT("Strength reduces"),
		UExcursionEncounterSubsystem::CalculateDeathChance(40.0f, 3, 0, 5.0f, 10.0f), 25.0f);

	// Each earlier bite raises it.
	TestEqual(TEXT("Prior bites increase"),
		UExcursionEncounterSubsystem::CalculateDeathChance(40.0f, 0, 2, 5.0f, 10.0f), 60.0f);

	// Both terms together.
	TestEqual(TEXT("Strength and bites combine"),
		UExcursionEncounterSubsystem::CalculateDeathChance(40.0f, 2, 1, 5.0f, 10.0f), 40.0f);

	// Clamped to 0-100.
	TestEqual(TEXT("Clamped at 0"),
		UExcursionEncounterSubsystem::CalculateDeathChance(10.0f, 10, 0, 5.0f, 10.0f), 0.0f);
	TestEqual(TEXT("Clamped at 100"),
		UExcursionEncounterSubsystem::CalculateDeathChance(90.0f, 0, 5, 5.0f, 10.0f), 100.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
