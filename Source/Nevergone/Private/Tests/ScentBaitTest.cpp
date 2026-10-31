// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Types/ScentBaitRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneScentBaitLifeTest, "Nevergone.Excursion.ScentBait.Life",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneScentBaitLifeTest::RunTest(const FString& Parameters)
{
	// Left alone: the ground life runs out.
	{
		FScentBaitLife Life;
		Life.Start(10.0f);

		TestFalse(TEXT("Not eaten at the start"), Life.IsBeingEaten());
		TestFalse(TEXT("Still there after 9.5s"), Life.Advance(9.5f));
		TestTrue(TEXT("Gone after 10s with nobody"), Life.Advance(0.5f));
	}

	// The first eater swaps the remaining ground life for the eating time.
	{
		FScentBaitLife Life;
		Life.Start(10.0f);

		Life.Advance(9.0f);

		TestTrue(TEXT("The first eater starts the eating time"), Life.StartEating(5.0f));
		TestTrue(TEXT("Being eaten"), Life.IsBeingEaten());
		TestEqual(TEXT("Eating time replaces the ground time"), Life.GetSecondsLeft(), 5.0f);

		Life.Advance(3.0f);

		// A second enemy joining does not extend it.
		TestFalse(TEXT("A later eater does not restart it"), Life.StartEating(5.0f));
		TestEqual(TEXT("Time left unchanged by a later eater"), Life.GetSecondsLeft(), 2.0f);

		TestFalse(TEXT("Still there 1s before the end"), Life.Advance(1.0f));
		TestTrue(TEXT("Eaten up"), Life.Advance(1.0f));
	}

	// Frozen by a capture: dilated delta time is zero.
	{
		FScentBaitLife Life;
		Life.Start(1.0f);

		for (int32 Frame = 0; Frame < 100; ++Frame)
		{
			Life.Advance(0.0f);
		}

		TestEqual(TEXT("A frozen bait does not age"), Life.GetSecondsLeft(), 1.0f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneScentBaitRulesTest, "Nevergone.Excursion.ScentBait.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneScentBaitRulesTest::RunTest(const FString& Parameters)
{
	using EState = EExplorationEnemyState;

	// Pulled out of a patrol (standing guard too) and an investigation, once
	// the first patrol has started.
	TestFalse(TEXT("Idle (before the first patrol) is not baited"), ScentBaitRules::CanBeBaited(EState::Idle));
	TestTrue(TEXT("Patrol is baited"), ScentBaitRules::CanBeBaited(EState::Patrol));
	TestTrue(TEXT("Investigate is baited"), ScentBaitRules::CanBeBaited(EState::Investigate));

	// A chase and a suspicion pass the bait by; the rest cannot act.
	TestFalse(TEXT("Chase ignores the bait"), ScentBaitRules::CanBeBaited(EState::Chase));
	TestFalse(TEXT("Suspicious ignores the bait"), ScentBaitRules::CanBeBaited(EState::Suspicious));
	TestFalse(TEXT("Already baited"), ScentBaitRules::CanBeBaited(EState::Baited));
	TestFalse(TEXT("Catch ignores the bait"), ScentBaitRules::CanBeBaited(EState::Catch));
	TestFalse(TEXT("KnockedDown ignores the bait"), ScentBaitRules::CanBeBaited(EState::KnockedDown));
	TestFalse(TEXT("Dead ignores the bait"), ScentBaitRules::CanBeBaited(EState::Dead));

	// Sight on the way, blind while eating.
	TestTrue(TEXT("Sees on the way to the bait"), ScentBaitRules::CanSee(EState::Baited, false));
	TestFalse(TEXT("Blind while eating"), ScentBaitRules::CanSee(EState::Baited, true));
	TestTrue(TEXT("Sees while patrolling"), ScentBaitRules::CanSee(EState::Patrol, false));

	// Deaf for the whole bait; a noise still moves a patrol or a search.
	TestFalse(TEXT("Deaf while baited"), ScentBaitRules::CanReactToNoise(EState::Baited));
	TestTrue(TEXT("Patrol hears"), ScentBaitRules::CanReactToNoise(EState::Patrol));
	TestTrue(TEXT("Investigate hears"), ScentBaitRules::CanReactToNoise(EState::Investigate));
	TestTrue(TEXT("Idle hears"), ScentBaitRules::CanReactToNoise(EState::Idle));
	TestFalse(TEXT("Suspicious holds noise instead"), ScentBaitRules::CanReactToNoise(EState::Suspicious));
	TestFalse(TEXT("Chase keeps its target"), ScentBaitRules::CanReactToNoise(EState::Chase));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
