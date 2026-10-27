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

	// Scenery keeps the channel's default and muffles.
	if (ResponseOf(TEXT("BlockAll"), Response))
	{
		TestTrue(TEXT("Wall (BlockAll) blocks noise occlusion"), Response == ECR_Block);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
