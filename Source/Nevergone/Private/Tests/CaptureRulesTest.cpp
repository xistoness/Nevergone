// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Types/CaptureTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneCaptureLineTest, "Nevergone.Excursion.CaptureLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneCaptureLineTest::RunTest(const FString& Parameters)
{
	// The door and the line decide together.
	TestTrue(TEXT("Nothing between them: a free line"),
		CaptureRules::ResolveLineBlock(false, false) == ECaptureLineBlock::None);

	TestTrue(TEXT("Closed door across the doorway"),
		CaptureRules::ResolveLineBlock(true, false) == ECaptureLineBlock::ClosedDoor);

	TestTrue(TEXT("Wall between them (diagonal behind the frame, thin wall)"),
		CaptureRules::ResolveLineBlock(false, true) == ECaptureLineBlock::Scenery);

	TestTrue(TEXT("Both: the door is the one to go through"),
		CaptureRules::ResolveLineBlock(true, true) == ECaptureLineBlock::ClosedDoor);

	TestTrue(TEXT("Within distance, free line: catch"),
		CaptureRules::CanCatch(true, ECaptureLineBlock::None));

	TestFalse(TEXT("Within distance, closed door: no catch"),
		CaptureRules::CanCatch(true, ECaptureLineBlock::ClosedDoor));

	TestFalse(TEXT("Within distance, wall: no catch"),
		CaptureRules::CanCatch(true, ECaptureLineBlock::Scenery));

	TestFalse(TEXT("Out of distance, free line: no catch"),
		CaptureRules::CanCatch(false, ECaptureLineBlock::None));

	// What blocks the line: fixed scenery only.
	TestTrue(TEXT("Wall (WorldStatic) blocks"),
		CaptureRules::IsSceneryHit(ECC_WorldStatic, false));

	TestTrue(TEXT("Fixed movable furniture (WorldDynamic) blocks"),
		CaptureRules::IsSceneryHit(ECC_WorldDynamic, false));

	TestFalse(TEXT("Closed door's box: decided by the door's state, not the line"),
		CaptureRules::IsSceneryHit(ECC_WorldStatic, true));

	TestFalse(TEXT("Door leaf, open or closed, does not block"),
		CaptureRules::IsSceneryHit(ECC_WorldDynamic, true));

	TestFalse(TEXT("Knockable prop (PhysicsBody) does not block"),
		CaptureRules::IsSceneryHit(ECC_PhysicsBody, false));

	TestFalse(TEXT("Character (Pawn) does not block"),
		CaptureRules::IsSceneryHit(ECC_Pawn, false));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
