// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Types/ExplorationSuspicion.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneExplorationSuspicionToleranceTest, "Nevergone.Excursion.SuspicionTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneExplorationSuspicionToleranceTest::RunTest(const FString& Parameters)
{
	using EStep = EExplorationSuspicionStep;

	constexpr float Tolerance = 0.5f;

	const FVector SeenA(100.0f, 0.0f, 0.0f);
	const FVector SeenB(200.0f, 0.0f, 0.0f);

	// --- A short loss pauses the confirmation and resumes it ---
	{
		FExplorationSuspicion Suspicion;

		Suspicion.AddContact(0.1f);
		Suspicion.Open(SeenA);

		TestTrue(TEXT("In view: watching"), Suspicion.Advance(true, SeenA, 0.2f, Tolerance) == EStep::Watching);
		TestEqual(TEXT("Contact grows in view"), Suspicion.GetContactSeconds(), 0.3f, KINDA_SMALL_NUMBER);

		TestTrue(TEXT("Out of view: loss starts"), Suspicion.Advance(false, FVector::ZeroVector, 0.2f, Tolerance) == EStep::LostSight);
		TestTrue(TEXT("Still out of view: tolerating"), Suspicion.Advance(false, FVector::ZeroVector, 0.2f, Tolerance) == EStep::Tolerating);
		TestEqual(TEXT("Contact paused while out of view"), Suspicion.GetContactSeconds(), 0.3f, KINDA_SMALL_NUMBER);
		TestEqual(TEXT("Last seen point kept"), Suspicion.GetLastSeenLocation(), SeenA);

		TestTrue(TEXT("Back in view: regained"), Suspicion.Advance(true, SeenB, 0.2f, Tolerance) == EStep::Regained);
		TestEqual(TEXT("Contact resumes where it stopped"), Suspicion.GetContactSeconds(), 0.5f, KINDA_SMALL_NUMBER);
		TestFalse(TEXT("Regained clears the loss"), Suspicion.IsSightLost());

		// The gap restarts from zero after a regain.
		TestTrue(TEXT("Second loss starts fresh"), Suspicion.Advance(false, FVector::ZeroVector, 0.4f, Tolerance) == EStep::LostSight);
	}

	// --- A loss as long as the tolerance ends the window at the last seen point ---
	{
		FExplorationSuspicion Suspicion;

		Suspicion.Open(SeenA);
		Suspicion.Advance(true, SeenB, 0.1f, Tolerance);

		Suspicion.Advance(false, FVector::ZeroVector, 0.3f, Tolerance);

		TestTrue(TEXT("Past the tolerance: expired"), Suspicion.Advance(false, FVector::ZeroVector, 0.3f, Tolerance) == EStep::Expired);
		TestEqual(TEXT("Expired without noise: last seen point"), Suspicion.ResolveStimulus(), SeenB);
	}

	// --- No tolerance behaves like the old rule: the first frame out ends it ---
	{
		FExplorationSuspicion Suspicion;

		Suspicion.Open(SeenA);

		TestTrue(TEXT("Zero tolerance: expired at once"), Suspicion.Advance(false, FVector::ZeroVector, 0.016f, 0.0f) == EStep::Expired);
	}

	// --- Held noise ---
	{
		const FVector NoiseA(0.0f, 500.0f, 0.0f);
		const FVector NoiseB(0.0f, 900.0f, 0.0f);

		// Heard while the target is in view, then the target is lost: the
		// noise is the most recent stimulus.
		FExplorationSuspicion Suspicion;

		Suspicion.Open(SeenA);
		Suspicion.HoldNoise(NoiseA);

		TestTrue(TEXT("Noise held"), Suspicion.HasHeldNoise());
		TestTrue(TEXT("Noise does not end the window"), Suspicion.Advance(true, SeenA, 0.1f, Tolerance) == EStep::Watching);

		Suspicion.HoldNoise(NoiseB);

		TestEqual(TEXT("Only the most recent noise is held"), Suspicion.GetHeldNoiseLocation(), NoiseB);

		Suspicion.Advance(false, FVector::ZeroVector, 0.3f, Tolerance);
		Suspicion.Advance(false, FVector::ZeroVector, 0.3f, Tolerance);

		TestEqual(TEXT("Expired with a newer noise: the noise"), Suspicion.ResolveStimulus(), NoiseB);

		// Confirming discards the noise.
		Suspicion.DiscardHeldNoise();

		TestFalse(TEXT("Discarded noise is gone"), Suspicion.HasHeldNoise());
		TestEqual(TEXT("Without the noise: last seen point"), Suspicion.ResolveStimulus(), SeenA);

		// Heard during the tolerance, then the target is seen again: the
		// sighting is newer, so the noise is dropped.
		FExplorationSuspicion Regained;

		Regained.Open(SeenA);
		Regained.Advance(false, FVector::ZeroVector, 0.1f, Tolerance);
		Regained.HoldNoise(NoiseA);
		Regained.Advance(true, SeenB, 0.1f, Tolerance);

		TestFalse(TEXT("Seeing the target again drops the noise"), Regained.HasHeldNoise());

		Regained.Advance(false, FVector::ZeroVector, 0.6f, Tolerance);

		TestEqual(TEXT("Expired after a regain: last seen point"), Regained.ResolveStimulus(), SeenB);

		// A target acquired outside a window restarts the confirmation.
		FExplorationSuspicion Acquired;

		Acquired.AddContact(0.4f);
		Acquired.RestartContact();

		TestEqual(TEXT("Acquiring outside a window restarts the confirmation"), Acquired.GetContactSeconds(), 0.0f);

		// Inside a window, another survivor in view after the target is lost
		// keeps the confirmation and ends the gap.
		FExplorationSuspicion Switched;

		Switched.AddContact(0.2f);
		Switched.Open(SeenA);
		Switched.Advance(true, SeenA, 0.2f, Tolerance);
		Switched.Advance(false, FVector::ZeroVector, 0.2f, Tolerance);

		TestTrue(TEXT("Target lost before the switch"), Switched.IsSightLost());

		Switched.SwitchTarget(SeenB);

		TestEqual(TEXT("Switching keeps the confirmation"), Switched.GetContactSeconds(), 0.4f, KINDA_SMALL_NUMBER);
		TestFalse(TEXT("Switching ends the sight loss"), Switched.IsSightLost());
		TestEqual(TEXT("Switching moves the last seen point"), Switched.GetLastSeenLocation(), SeenB);
		TestTrue(TEXT("The new target is watched, not regained"), Switched.Advance(true, SeenB, 0.1f, Tolerance) == EStep::Watching);
		TestEqual(TEXT("The confirmation keeps growing"), Switched.GetContactSeconds(), 0.5f, KINDA_SMALL_NUMBER);

		// A noise held before the switch is older than the new sighting.
		FExplorationSuspicion SwitchedAfterNoise;

		SwitchedAfterNoise.Open(SeenA);
		SwitchedAfterNoise.Advance(false, FVector::ZeroVector, 0.1f, Tolerance);
		SwitchedAfterNoise.HoldNoise(NoiseA);
		SwitchedAfterNoise.SwitchTarget(SeenB);

		TestFalse(TEXT("Switching drops the earlier noise"), SwitchedAfterNoise.HasHeldNoise());

		SwitchedAfterNoise.Advance(true, SeenB, 0.1f, Tolerance);
		SwitchedAfterNoise.Advance(false, FVector::ZeroVector, 0.6f, Tolerance);

		TestEqual(TEXT("Expired after a switch: the new target's last seen point"), SwitchedAfterNoise.ResolveStimulus(), SeenB);

		// A noise heard after the switch is the most recent stimulus.
		FExplorationSuspicion NoiseAfterSwitch;

		NoiseAfterSwitch.Open(SeenA);
		NoiseAfterSwitch.Advance(false, FVector::ZeroVector, 0.1f, Tolerance);
		NoiseAfterSwitch.SwitchTarget(SeenB);
		NoiseAfterSwitch.Advance(true, SeenB, 0.1f, Tolerance);
		NoiseAfterSwitch.HoldNoise(NoiseB);
		NoiseAfterSwitch.Advance(false, FVector::ZeroVector, 0.6f, Tolerance);

		TestEqual(TEXT("Noise heard after the switch: the noise"), NoiseAfterSwitch.ResolveStimulus(), NoiseB);

		// A new window starts without the previous one's noise.
		FExplorationSuspicion Reopened;

		Reopened.Open(SeenA);
		Reopened.HoldNoise(NoiseA);
		Reopened.Open(SeenB);

		TestFalse(TEXT("Opening a window clears the held noise"), Reopened.HasHeldNoise());

		Reopened.HoldNoise(NoiseA);
		Reopened.Reset();

		TestFalse(TEXT("Reset clears the held noise"), Reopened.HasHeldNoise());
		TestEqual(TEXT("Reset clears the contact"), Reopened.GetContactSeconds(), 0.0f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
