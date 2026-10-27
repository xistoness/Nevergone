// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Engine/CollisionProfile.h"
#include "EngineTools/CollisionChannels.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneNoiseOcclusionProfilesTest, "Nevergone.Excursion.NoiseOcclusionProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneNoiseOcclusionProfilesTest::RunTest(const FString& Parameters)
{
	const UCollisionProfile* Profiles = UCollisionProfile::Get();

	auto ResponseOf = [this, Profiles](FName ProfileName, ECollisionResponse& OutResponse)
	{
		FCollisionResponseTemplate Template;

		if (!Profiles->GetProfileTemplate(ProfileName, Template))
		{
			AddError(FString::Printf(TEXT("Collision profile %s not found"), *ProfileName.ToString()));

			return false;
		}

		OutResponse = Template.ResponseToChannels.GetResponse(NevergoneCollision::NoiseOcclusion);

		return true;
	};

	ECollisionResponse Response = ECR_Block;

	// Characters, alive or dead, never muffle noise: the capsule, the mesh
	// and the corpse's ragdoll all ignore the channel.
	if (ResponseOf(TEXT("Pawn"), Response))
	{
		TestTrue(TEXT("Character capsule (Pawn) ignores noise occlusion"), Response == ECR_Ignore);
	}

	if (ResponseOf(TEXT("CharacterMesh"), Response))
	{
		TestTrue(TEXT("Character mesh (CharacterMesh) ignores noise occlusion"), Response == ECR_Ignore);
	}

	if (ResponseOf(TEXT("Ragdoll"), Response))
	{
		TestTrue(TEXT("Corpse (Ragdoll) ignores noise occlusion"), Response == ECR_Ignore);
	}

	// Nothing the player cannot see muffles: triggers, blocking volumes,
	// generic volumes, widgets and spectators all ignore the channel.
	static const TCHAR* InvisibleProfiles[] =
	{
		TEXT("Trigger"),
		TEXT("InvisibleWall"),
		TEXT("InvisibleWallDynamic"),
		TEXT("UI"),
		TEXT("OverlapAll"),
		TEXT("OverlapAllDynamic"),
		TEXT("OverlapOnlyPawn"),
		TEXT("Spectator"),
	};

	for (const TCHAR* ProfileName : InvisibleProfiles)
	{
		if (ResponseOf(ProfileName, Response))
		{
			TestTrue(FString::Printf(TEXT("%s ignores noise occlusion"), ProfileName), Response == ECR_Ignore);
		}
	}

	// Scenery keeps the channel's default and muffles.
	static const TCHAR* SceneryProfiles[] =
	{
		TEXT("BlockAll"),
		TEXT("BlockAllDynamic"),
	};

	for (const TCHAR* ProfileName : SceneryProfiles)
	{
		if (ResponseOf(ProfileName, Response))
		{
			TestTrue(FString::Printf(TEXT("Wall (%s) blocks noise occlusion"), ProfileName), Response == ECR_Block);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
