// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Nevergone.h"
#include "Characters/PlayerControllers/ExplorationEnemyController.h"
#include "Data/NoiseDefinition.h"
#include "EngineTools/AINavLinks.h"
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

	TestTrue(TEXT("Latched: barred, nobody opens it"),
		DoorRules::ResolveAgentPassage(EDoorState::Latched) == EDoorAgentPassage::Barred);

	TestTrue(TEXT("Broken: walk through"),
		DoorRules::ResolveAgentPassage(EDoorState::Broken) == EDoorAgentPassage::Pass);

	TestTrue(TEXT("Link on while open"), DoorRules::IsAgentLinkEnabled(EDoorState::Open));
	TestTrue(TEXT("Link on while closed"), DoorRules::IsAgentLinkEnabled(EDoorState::Closed));
	TestTrue(TEXT("Link on while latched: agents reach it and knock"), DoorRules::IsAgentLinkEnabled(EDoorState::Latched));
	TestTrue(TEXT("Link on while broken"), DoorRules::IsAgentLinkEnabled(EDoorState::Broken));
	TestFalse(TEXT("Link off while locked"), DoorRules::IsAgentLinkEnabled(EDoorState::Locked));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneDoorLatchSideTest, "Nevergone.Excursion.DoorLatchSide",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneDoorLatchSideTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Front latch, opener in front"), DoorRules::IsOnLatchSide(EDoorLatchSide::Front, 50.0f));
	TestFalse(TEXT("Front latch, opener behind"), DoorRules::IsOnLatchSide(EDoorLatchSide::Front, -50.0f));
	TestTrue(TEXT("Back latch, opener behind"), DoorRules::IsOnLatchSide(EDoorLatchSide::Back, -50.0f));
	TestFalse(TEXT("Back latch, opener in front"), DoorRules::IsOnLatchSide(EDoorLatchSide::Back, 50.0f));
	TestFalse(TEXT("No latch: no latch side"), DoorRules::IsOnLatchSide(EDoorLatchSide::None, 50.0f));
	TestFalse(TEXT("In the doorway plane: neither side"), DoorRules::IsOnLatchSide(EDoorLatchSide::Front, 0.0f));

	TestTrue(TEXT("Hold on a closed door, latch side"), DoorRules::CanHoldInteract(EDoorState::Closed, true));
	TestTrue(TEXT("Hold on an open door, latch side"), DoorRules::CanHoldInteract(EDoorState::Open, true));
	TestTrue(TEXT("Hold on a latched door, latch side"), DoorRules::CanHoldInteract(EDoorState::Latched, true));
	TestFalse(TEXT("No hold from the other side"), DoorRules::CanHoldInteract(EDoorState::Closed, false));
	TestFalse(TEXT("No hold on a key door"), DoorRules::CanHoldInteract(EDoorState::Locked, true));
	TestFalse(TEXT("No hold on a broken door"), DoorRules::CanHoldInteract(EDoorState::Broken, true));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneDoorInteractionTest, "Nevergone.Excursion.DoorInteraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneDoorInteractionTest::RunTest(const FString& Parameters)
{
	auto Check = [this](const TCHAR* What, EDoorState State, bool bLatchSide, bool bHold, EDoorInteraction Expected)
	{
		TestTrue(What, DoorRules::ResolveInteraction(State, bLatchSide, bHold) == Expected);
	};

	// Taps open and close, from either side.
	Check(TEXT("Tap closed, latch side: open"), EDoorState::Closed, true, false, EDoorInteraction::Open);
	Check(TEXT("Tap closed, other side: open"), EDoorState::Closed, false, false, EDoorInteraction::Open);
	Check(TEXT("Tap open: close"), EDoorState::Open, false, false, EDoorInteraction::Close);
	Check(TEXT("Tap open, latch side: close"), EDoorState::Open, true, false, EDoorInteraction::Close);

	// Holds work the latch from the latch side.
	Check(TEXT("Hold closed, latch side: latch"), EDoorState::Closed, true, true, EDoorInteraction::Latch);
	Check(TEXT("Hold open, latch side: close and latch"), EDoorState::Open, true, true, EDoorInteraction::CloseAndLatch);
	Check(TEXT("Hold latched, latch side: unlatch"), EDoorState::Latched, true, true, EDoorInteraction::Unlatch);
	Check(TEXT("Hold from the other side acts as a tap"), EDoorState::Closed, false, true, EDoorInteraction::Open);
	Check(TEXT("Hold open, other side: close"), EDoorState::Open, false, true, EDoorInteraction::Close);

	// A latched door opens from the latch side only.
	Check(TEXT("Tap latched, latch side: unlatch and open"), EDoorState::Latched, true, false, EDoorInteraction::UnlatchAndOpen);
	Check(TEXT("Tap latched, other side: jolt"), EDoorState::Latched, false, false, EDoorInteraction::Jolt);
	Check(TEXT("Hold latched, other side: jolt"), EDoorState::Latched, false, true, EDoorInteraction::Jolt);

	Check(TEXT("Key door: the key"), EDoorState::Locked, true, true, EDoorInteraction::UseKey);
	Check(TEXT("Broken: nothing"), EDoorState::Broken, true, false, EDoorInteraction::None);
	Check(TEXT("Broken, held: nothing"), EDoorState::Broken, true, true, EDoorInteraction::None);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneDoorJoltTest, "Nevergone.Excursion.DoorJolt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneDoorJoltTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Starts at rest"), DoorRules::GetJoltOffset(0.0f, 0.3f, 8.0f), 0.0f);
	TestEqual(TEXT("Catches at half the duration"), DoorRules::GetJoltOffset(0.15f, 0.3f, 8.0f), 8.0f, 0.001f);
	TestEqual(TEXT("Halfway out"), DoorRules::GetJoltOffset(0.075f, 0.3f, 8.0f), 4.0f, 0.001f);
	TestEqual(TEXT("Halfway back"), DoorRules::GetJoltOffset(0.225f, 0.3f, 8.0f), 4.0f, 0.001f);
	TestEqual(TEXT("Back at rest at the end"), DoorRules::GetJoltOffset(0.3f, 0.3f, 8.0f), 0.0f);
	TestEqual(TEXT("At rest after the end"), DoorRules::GetJoltOffset(1.0f, 0.3f, 8.0f), 0.0f);
	TestEqual(TEXT("No duration: no jolt"), DoorRules::GetJoltOffset(0.1f, 0.0f, 8.0f), 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneDoorKnockTest, "Nevergone.Excursion.DoorKnock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneDoorKnockTest::RunTest(const FString& Parameters)
{
	// Knocks every second, breaks after five, stepped at 10 fps.
	FDoorKnock Knock;

	int32 Knocks = 0;
	int32 Steps = 0;
	bool bBroke = false;

	TestFalse(TEXT("Not started before the first step"), Knock.HasStarted());

	while (!bBroke && Steps < 1000)
	{
		const FDoorKnock::FStep Step = Knock.Advance(0.1f, 1.0f, 5.0f);

		++Steps;

		if (Steps == 1)
		{
			TestTrue(TEXT("Knocks on arrival"), Step.bKnock);
		}

		Knocks += Step.bKnock ? 1 : 0;
		bBroke = Step.bBreak;

		if (bBroke)
		{
			TestFalse(TEXT("The breaking step is not a knock"), Step.bKnock);
		}
	}

	TestTrue(TEXT("Breaks"), bBroke);
	TestEqual(TEXT("Five knocks before it breaks"), Knocks, 5);
	TestTrue(TEXT("Breaks at five seconds"), FMath::IsNearlyEqual(Knock.GetElapsedSeconds(), 5.0f, 0.11f));

	// A frozen enemy (zero delta) never gets closer to breaking it.
	FDoorKnock Frozen;

	Frozen.Advance(0.0f, 1.0f, 5.0f);

	for (int32 Index = 0; Index < 100; ++Index)
	{
		TestFalse(TEXT("Zero time never breaks"), Frozen.Advance(0.0f, 1.0f, 5.0f).bBreak);
	}

	TestEqual(TEXT("Zero time adds nothing"), Frozen.GetElapsedSeconds(), 0.0f);

	// One long frame plays a single knock, not a burst.
	FDoorKnock Long;

	Long.Advance(0.0f, 1.0f, 50.0f);

	TestTrue(TEXT("Long frame: one knock"), Long.Advance(3.5f, 1.0f, 50.0f).bKnock);
	TestFalse(TEXT("Next short frame: no catch-up knock"), Long.Advance(0.1f, 1.0f, 50.0f).bKnock);

	// No break time: breaks on arrival, without a knock.
	FDoorKnock Instant;

	const FDoorKnock::FStep InstantStep = Instant.Advance(0.1f, 1.0f, 0.0f);

	TestTrue(TEXT("Zero break time: breaks at once"), InstantStep.bBreak);
	TestFalse(TEXT("Zero break time: no knock"), InstantStep.bKnock);

	// A zero interval is clamped, so knocks never come every frame.
	FDoorKnock Fast;

	TestTrue(TEXT("Zero interval: knocks on arrival"), Fast.Advance(0.0f, 0.0f, 50.0f).bKnock);
	TestFalse(TEXT("Zero interval: no knock inside the minimum"), Fast.Advance(0.05f, 0.0f, 50.0f).bKnock);
	TestTrue(TEXT("Zero interval: knocks after the minimum"), Fast.Advance(0.07f, 0.0f, 50.0f).bKnock);

	Knock.Reset();

	TestFalse(TEXT("Reset starts over"), Knock.HasStarted());
	TestEqual(TEXT("Reset clears the time"), Knock.GetElapsedSeconds(), 0.0f);

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

	TestTrue(TEXT("Knocking at a door: place noise"),
		UExcursionNoiseSubsystem::IsHeardAsPlaceNoise(TAG_Noise_DoorKnock.GetTag().GetTagName(), false));

	TestTrue(TEXT("Breaking a door: place noise"),
		UExcursionNoiseSubsystem::IsHeardAsPlaceNoise(TAG_Noise_DoorBreak.GetTag().GetTagName(), false));

	TestFalse(TEXT("Own knocking: not heard"),
		UExcursionNoiseSubsystem::IsHeardAsPlaceNoise(TAG_Noise_DoorKnock.GetTag().GetTagName(), true));

	TestFalse(TEXT("Footstep: not a place noise"),
		UExcursionNoiseSubsystem::IsHeardAsPlaceNoise(TAG_Noise_Footstep.GetTag().GetTagName(), false));

	TestFalse(TEXT("Untagged: not a place noise"),
		UExcursionNoiseSubsystem::IsHeardAsPlaceNoise(NAME_None, false));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneNoiseSourceLogTest, "Nevergone.Excursion.NoiseSourceLog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneNoiseSourceLogTest::RunTest(const FString& Parameters)
{
	// Plain objects stand in for the instigator and the door.
	const UObject* Enemy = NewObject<UNoiseDefinition>();
	const UObject* OtherEnemy = NewObject<UNoiseDefinition>();
	const UObject* Door = NewObject<UNoiseDefinition>();

	const FVector At(100.0, 200.0, 50.0);
	const FName Knock = TAG_Noise_DoorKnock.GetTag().GetTagName();
	const FName Break = TAG_Noise_DoorBreak.GetTag().GetTagName();

	FExcursionNoiseSourceLog Log;

	Log.Add(Enemy, At, Knock, Door, 10.0);

	TestTrue(TEXT("Same instigator, place and tag: the door"), Log.Find(Enemy, At, Knock, 10.0) == Door);
	TestTrue(TEXT("Rounding in the location still matches"), Log.Find(Enemy, At + FVector(0.5, 0.0, 0.0), Knock, 10.0) == Door);
	TestNull(TEXT("Another instigator: no source"), Log.Find(OtherEnemy, At, Knock, 10.0));
	TestNull(TEXT("Another place: no source"), Log.Find(Enemy, At + FVector(50.0, 0.0, 0.0), Knock, 10.0));
	TestNull(TEXT("Another tag: no source"), Log.Find(Enemy, At, Break, 10.0));
	TestNull(TEXT("Too old: no source"), Log.Find(Enemy, At, Knock, 10.0 + FExcursionNoiseSourceLog::LifetimeSeconds + 0.1));

	// The same noise made again: the newest emission wins.
	UObject* OtherDoor = NewObject<UNoiseDefinition>();

	Log.Add(Enemy, At, Knock, OtherDoor, 10.2);

	TestTrue(TEXT("Newest emission first"), Log.Find(Enemy, At, Knock, 10.3) == OtherDoor);

	// A source destroyed since is no source.
	OtherDoor->MarkAsGarbage();

	TestNull(TEXT("Destroyed source: no source"), Log.Find(Enemy, At, Knock, 10.3));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneDoorCaptureBlockTest, "Nevergone.Excursion.DoorCaptureBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneDoorCaptureBlockTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Closed door blocks the capture"), DoorRules::DoesDoorBlockCapture(EDoorState::Closed));
	TestTrue(TEXT("Latched door blocks the capture"), DoorRules::DoesDoorBlockCapture(EDoorState::Latched));
	TestTrue(TEXT("Locked door blocks the capture"), DoorRules::DoesDoorBlockCapture(EDoorState::Locked));
	TestFalse(TEXT("Open door: the leaf hides, but does not block"), DoorRules::DoesDoorBlockCapture(EDoorState::Open));
	TestFalse(TEXT("Broken door does not block"), DoorRules::DoesDoorBlockCapture(EDoorState::Broken));

	// The default box: 10 thick, 120 wide, 200 tall, in its own frame.
	const FVector Extent(5.0, 60.0, 100.0);

	TestTrue(TEXT("Enemy and victim on either side, across the doorway"),
		DoorRules::DoesSegmentCrossDoorway(FVector(-80.0, 0.0, -10.0), FVector(45.0, 10.0, -10.0), Extent));

	TestTrue(TEXT("Crossing at an angle, still within the width"),
		DoorRules::DoesSegmentCrossDoorway(FVector(-60.0, -80.0, 0.0), FVector(60.0, 80.0, 0.0), Extent));

	TestFalse(TEXT("Both on the same side"),
		DoorRules::DoesSegmentCrossDoorway(FVector(-80.0, 0.0, 0.0), FVector(-30.0, 20.0, 0.0), Extent));

	TestFalse(TEXT("Across the plane but beside the doorway (through the wall)"),
		DoorRules::DoesSegmentCrossDoorway(FVector(-50.0, 90.0, 0.0), FVector(50.0, 90.0, 0.0), Extent));

	TestFalse(TEXT("Across the plane above the doorway"),
		DoorRules::DoesSegmentCrossDoorway(FVector(-50.0, 0.0, 150.0), FVector(50.0, 0.0, 150.0), Extent));

	TestFalse(TEXT("A point on the plane is on neither side"),
		DoorRules::DoesSegmentCrossDoorway(FVector(0.0, 0.0, 0.0), FVector(60.0, 0.0, 0.0), Extent));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneDoorwayStopTest, "Nevergone.Excursion.DoorwayStop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneDoorwayStopTest::RunTest(const FString& Parameters)
{
	auto Waits = [](EExplorationEnemyState State)
	{
		return AExplorationEnemyController::DoesStateWaitForDoorway(State);
	};

	// The states that stop the enemy or send it elsewhere start once across.
	TestTrue(TEXT("Idle waits for the doorway"), Waits(EExplorationEnemyState::Idle));
	TestTrue(TEXT("Giving up (Patrol) waits for the doorway"), Waits(EExplorationEnemyState::Patrol));
	TestTrue(TEXT("Suspicious waits for the doorway"), Waits(EExplorationEnemyState::Suspicious));
	TestTrue(TEXT("Investigate waits for the doorway"), Waits(EExplorationEnemyState::Investigate));

	// A chase must still catch; the capture states place the body.
	TestFalse(TEXT("Chase starts at once"), Waits(EExplorationEnemyState::Chase));
	TestFalse(TEXT("Catch starts at once"), Waits(EExplorationEnemyState::Catch));
	TestFalse(TEXT("Knockdown starts at once"), Waits(EExplorationEnemyState::KnockedDown));
	TestFalse(TEXT("Dead starts at once"), Waits(EExplorationEnemyState::Dead));

	// A finished move walks on out of the doorway only when it arrived
	// inside the doorway of the door the agent crossed.
	TestTrue(TEXT("Arrived halfway through the door crossed: walk out"),
		FDoorwayCrossing::ShouldWalkOut(true, true, true));
	TestFalse(TEXT("Arrived past the doorway: done"),
		FDoorwayCrossing::ShouldWalkOut(true, true, false));
	TestFalse(TEXT("Aborted in the doorway (stopped on purpose): stays"),
		FDoorwayCrossing::ShouldWalkOut(true, false, true));
	TestFalse(TEXT("Never crossed a door: nothing to walk out of"),
		FDoorwayCrossing::ShouldWalkOut(false, true, true));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
