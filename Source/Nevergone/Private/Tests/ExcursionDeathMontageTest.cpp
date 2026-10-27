// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Animation/AnimMontage.h"
#include "Data/ExplorationEnemyDefinition.h"
#include "World/ExcursionEncounterSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneExcursionDeathMontageTest, "Nevergone.Excursion.DeathMontage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneExcursionDeathMontageTest::RunTest(const FString& Parameters)
{
	UExplorationEnemyDefinition* Definition = NewObject<UExplorationEnemyDefinition>();

	// Every slot gets its own montage, so a wrong pick cannot pass.
	auto NewMontage = []() { return NewObject<UAnimMontage>(); };

	Definition->DevourFollowerAnimation.EnemyMontage = NewMontage();
	Definition->DevourFollowerAnimation.VictimMontage = NewMontage();
	Definition->SafeguardAnimation.EnemyMontage = NewMontage();
	Definition->SafeguardAnimation.VictimMontage = NewMontage();
	Definition->BiteAnimation.EnemyMontage = NewMontage();
	Definition->BiteAnimation.VictimMontage = NewMontage();
	Definition->KillAnimation.EnemyMontage = NewMontage();
	Definition->KillAnimation.VictimMontage = NewMontage();

	TestTrue(TEXT("Devoured follower dies in the follower's devour montage"),
		UExcursionEncounterSubsystem::GetDeathMontage(*Definition, false, EExcursionCaptureOutcome::Killed)
			== Definition->DevourFollowerAnimation.VictimMontage);

	TestTrue(TEXT("Safeguard: the enemy dies in its safeguard montage"),
		UExcursionEncounterSubsystem::GetDeathMontage(*Definition, true, EExcursionCaptureOutcome::Unharmed)
			== Definition->SafeguardAnimation.EnemyMontage);

	TestTrue(TEXT("Killed player dies in the kill montage's victim side"),
		UExcursionEncounterSubsystem::GetDeathMontage(*Definition, true, EExcursionCaptureOutcome::Killed)
			== Definition->KillAnimation.VictimMontage);

	TestNull(TEXT("Bite: nobody dies"),
		UExcursionEncounterSubsystem::GetDeathMontage(*Definition, true, EExcursionCaptureOutcome::Infected));

	// A type without the montage leaves the body without a pose to hold.
	Definition->KillAnimation.VictimMontage = nullptr;

	TestNull(TEXT("Kill without a victim montage"),
		UExcursionEncounterSubsystem::GetDeathMontage(*Definition, true, EExcursionCaptureOutcome::Killed));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
