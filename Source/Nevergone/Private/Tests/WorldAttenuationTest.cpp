// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Audio/AudioSubsystem.h"

#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneWorldAttenuationTest, "Nevergone.Audio.WorldAttenuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneWorldAttenuationTest::RunTest(const FString& Parameters)
{
	USoundAttenuation* Fallback = NewObject<USoundAttenuation>(GetTransientPackage());
	USoundAttenuation* Own = NewObject<USoundAttenuation>(GetTransientPackage());

	USoundWave* PlainSound = NewObject<USoundWave>(GetTransientPackage());

	USoundWave* SoundWithOwn = NewObject<USoundWave>(GetTransientPackage());
	SoundWithOwn->AttenuationSettings = Own;

	TestTrue(TEXT("Sound without attenuation: takes the default"),
		UAudioSubsystem::PickWorldAttenuation(PlainSound, Fallback) == Fallback);

	TestNull(TEXT("Sound with its own attenuation: keeps it (no override)"),
		UAudioSubsystem::PickWorldAttenuation(SoundWithOwn, Fallback));

	TestNull(TEXT("No default authored: nothing to apply"),
		UAudioSubsystem::PickWorldAttenuation(PlainSound, nullptr));

	TestNull(TEXT("No sound: nothing to apply"),
		UAudioSubsystem::PickWorldAttenuation(nullptr, Fallback));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
