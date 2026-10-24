// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "ActorComponents/ExplorationEnemySignalComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneExplorationEnemySignalTest, "Nevergone.Excursion.EnemySignals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneExplorationEnemySignalTest::RunTest(const FString& Parameters)
{
	using EState = EExplorationEnemyState;
	using EMoment = EExplorationEnemySignalMoment;

	auto Moment = [](EState From, EState To)
	{
		return UExplorationEnemySignalComponent::GetSignalMoment(From, To);
	};

	// Suspicion, from a patrol or mid-investigation.
	TestTrue(TEXT("Patrol -> Suspicious"), Moment(EState::Patrol, EState::Suspicious) == EMoment::Suspicion);
	TestTrue(TEXT("Investigate -> Suspicious"), Moment(EState::Investigate, EState::Suspicious) == EMoment::Suspicion);

	// Investigation, whatever led to it.
	TestTrue(TEXT("Patrol -> Investigate"), Moment(EState::Patrol, EState::Investigate) == EMoment::Investigation);
	TestTrue(TEXT("Suspicious -> Investigate"), Moment(EState::Suspicious, EState::Investigate) == EMoment::Investigation);
	TestTrue(TEXT("Chase -> Investigate"), Moment(EState::Chase, EState::Investigate) == EMoment::Investigation);
	TestTrue(TEXT("Catch -> Investigate (after feeding)"), Moment(EState::Catch, EState::Investigate) == EMoment::Investigation);

	// Every chase screams, getting up after a knockdown included.
	TestTrue(TEXT("Suspicious -> Chase"), Moment(EState::Suspicious, EState::Chase) == EMoment::Chase);
	TestTrue(TEXT("Patrol -> Chase (smell)"), Moment(EState::Patrol, EState::Chase) == EMoment::Chase);
	TestTrue(TEXT("KnockedDown -> Chase"), Moment(EState::KnockedDown, EState::Chase) == EMoment::Chase);

	// Giving up only after having noticed something.
	TestTrue(TEXT("Investigate -> Patrol"), Moment(EState::Investigate, EState::Patrol) == EMoment::GiveUp);
	TestTrue(TEXT("Chase -> Patrol"), Moment(EState::Chase, EState::Patrol) == EMoment::GiveUp);

	// Suspicion always ends in a chase or an investigation, so its patrol
	// transition carries no give-up.
	TestTrue(TEXT("Suspicious -> Patrol (unreachable)"), Moment(EState::Suspicious, EState::Patrol) == EMoment::None);
	TestTrue(TEXT("Idle -> Patrol (first patrol)"), Moment(EState::Idle, EState::Patrol) == EMoment::None);
	TestTrue(TEXT("KnockedDown -> Patrol"), Moment(EState::KnockedDown, EState::Patrol) == EMoment::None);
	TestTrue(TEXT("Catch -> Patrol (after feeding)"), Moment(EState::Catch, EState::Patrol) == EMoment::None);

	// The capture states have no signal of their own.
	TestTrue(TEXT("Chase -> Catch"), Moment(EState::Chase, EState::Catch) == EMoment::None);
	TestTrue(TEXT("Catch -> KnockedDown"), Moment(EState::Catch, EState::KnockedDown) == EMoment::None);
	TestTrue(TEXT("Catch -> Dead"), Moment(EState::Catch, EState::Dead) == EMoment::None);

	// No transition, no signal.
	TestTrue(TEXT("Investigate -> Investigate"), Moment(EState::Investigate, EState::Investigate) == EMoment::None);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
