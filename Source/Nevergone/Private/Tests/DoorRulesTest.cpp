// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Nevergone.h"
#include "Types/DoorTypes.h"
#include "World/ExcursionNoiseSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneDoorAgentPassageTest, "Nevergone.Excursion.DoorAgentPassage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneDoorAgentPassageTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Open: walk through"),
		DoorRules::ResolveAgentPassage(EDoorState::Open) == EDoorAgentPassage::Pass);

	TestTrue(TEXT("Closed: stop and open first"),
		DoorRules::ResolveAgentPassage(EDoorState::Closed) == EDoorAgentPassage::OpenFirst);

	TestTrue(TEXT("Locked key door: a wall"),
		DoorRules::ResolveAgentPassage(EDoorState::Locked) == EDoorAgentPassage::Blocked);

	TestTrue(TEXT("Link on while open"), DoorRules::IsAgentLinkEnabled(EDoorState::Open));
	TestTrue(TEXT("Link on while closed"), DoorRules::IsAgentLinkEnabled(EDoorState::Closed));
	TestFalse(TEXT("Link off while locked"), DoorRules::IsAgentLinkEnabled(EDoorState::Locked));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergonePlaceNoiseFilterTest, "Nevergone.Excursion.PlaceNoiseFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergonePlaceNoiseFilterTest::RunTest(const FString& Parameters)
{
	const FName Scream = TAG_Noise_Scream.GetTag().GetTagName();

	TestTrue(TEXT("Scream from somebody else: place noise"),
		UExcursionNoiseSubsystem::IsHeardAsPlaceNoise(Scream, false));

	TestFalse(TEXT("Own scream: not heard"),
		UExcursionNoiseSubsystem::IsHeardAsPlaceNoise(Scream, true));

	TestFalse(TEXT("Door noise: not a place noise"),
		UExcursionNoiseSubsystem::IsHeardAsPlaceNoise(TAG_Noise_Door.GetTag().GetTagName(), false));

	TestFalse(TEXT("Footstep: not a place noise"),
		UExcursionNoiseSubsystem::IsHeardAsPlaceNoise(TAG_Noise_Footstep.GetTag().GetTagName(), false));

	TestFalse(TEXT("Untagged: not a place noise"),
		UExcursionNoiseSubsystem::IsHeardAsPlaceNoise(NAME_None, false));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
