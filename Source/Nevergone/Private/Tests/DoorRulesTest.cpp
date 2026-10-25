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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneDoorSwingSideTest, "Nevergone.Excursion.DoorSwingSide",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneDoorSwingSideTest::RunTest(const FString& Parameters)
{
	// Doorway crossed along X. A positive yaw turns +Y towards -X.
	const FVector Axis = FVector::ForwardVector;

	const FVector LeafTowardsPlusY(0.0f, 45.0f, 0.0f);
	const FVector LeafTowardsMinusY(0.0f, -45.0f, 0.0f);

	// Leaf hinged so it extends to +Y: positive yaw swings it to -X.
	TestEqual(TEXT("+Y leaf, opener on +X: positive yaw"),
		DoorRules::ChooseSwingSign(100.0f, LeafTowardsPlusY, 90.0f, Axis), 1.0f);
	TestEqual(TEXT("+Y leaf, opener on -X: negative yaw"),
		DoorRules::ChooseSwingSign(-100.0f, LeafTowardsPlusY, 90.0f, Axis), -1.0f);

	// Hinge on the other side: the signs flip.
	TestEqual(TEXT("-Y leaf, opener on +X: negative yaw"),
		DoorRules::ChooseSwingSign(100.0f, LeafTowardsMinusY, 90.0f, Axis), -1.0f);
	TestEqual(TEXT("-Y leaf, opener on -X: positive yaw"),
		DoorRules::ChooseSwingSign(-100.0f, LeafTowardsMinusY, 90.0f, Axis), 1.0f);

	// Every swing lands the leaf on the side away from the opener.
	for (const float OpenerSide : { 100.0f, -100.0f })
	{
		for (const FVector& Leaf : { LeafTowardsPlusY, LeafTowardsMinusY })
		{
			const float Sign = DoorRules::ChooseSwingSign(OpenerSide, Leaf, 90.0f, Axis);
			const FVector Swung = FQuat(FVector::UpVector, FMath::DegreesToRadians(Sign * 90.0f)).RotateVector(Leaf);

			TestTrue(TEXT("Leaf ends opposite the opener"), FVector::DotProduct(Swung, Axis) * OpenerSide < 0.0f);
		}
	}

	// A wide swing picks the same side as a quarter turn.
	TestEqual(TEXT("180 deg swing: same side as 90"),
		DoorRules::ChooseSwingSign(100.0f, LeafTowardsPlusY, 180.0f, Axis), 1.0f);
	TestEqual(TEXT("180 deg swing, other side"),
		DoorRules::ChooseSwingSign(-100.0f, LeafTowardsPlusY, 180.0f, Axis), -1.0f);

	TestEqual(TEXT("Opener in the doorway plane: positive yaw"),
		DoorRules::ChooseSwingSign(0.0f, LeafTowardsPlusY, 90.0f, Axis), 1.0f);

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
