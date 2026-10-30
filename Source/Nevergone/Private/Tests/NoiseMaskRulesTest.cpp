// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Actors/ExcursionNoiseMaskVolume.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneNoiseMaskRulesTest, "Nevergone.Excursion.NoiseMaskRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneNoiseMaskRulesTest::RunTest(const FString& Parameters)
{
	// Any two distinct objects stand for the device and another noise source.
	const UObject* Device = GetTransientPackage();
	const UObject* OtherSource = UObject::StaticClass();

	TestTrue(TEXT("Volume without a device: masks any noise"),
		AExcursionNoiseMaskVolume::IsMaskApplied(true, nullptr, OtherSource));

	TestTrue(TEXT("Volume without a device: masks a noise without source"),
		AExcursionNoiseMaskVolume::IsMaskApplied(true, nullptr, nullptr));

	TestFalse(TEXT("Inactive volume (device off): masks nothing"),
		AExcursionNoiseMaskVolume::IsMaskApplied(false, Device, OtherSource));

	TestFalse(TEXT("Inactive volume: masks nothing, even without source"),
		AExcursionNoiseMaskVolume::IsMaskApplied(false, Device, nullptr));

	TestTrue(TEXT("Device on: masks a noise without source (footsteps)"),
		AExcursionNoiseMaskVolume::IsMaskApplied(true, Device, nullptr));

	TestTrue(TEXT("Device on: masks another object's noise"),
		AExcursionNoiseMaskVolume::IsMaskApplied(true, Device, OtherSource));

	TestFalse(TEXT("Device on: never masks its own noise"),
		AExcursionNoiseMaskVolume::IsMaskApplied(true, Device, Device));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
