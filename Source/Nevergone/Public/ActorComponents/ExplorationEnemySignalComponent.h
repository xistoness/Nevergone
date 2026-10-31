// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/ExplorationEnemyTypes.h"
#include "ExplorationEnemySignalComponent.generated.h"

class ADoor;
class AExplorationEnemyCharacter;
class UAudioComponent;
class UExplorationEnemyDefinition;
class USoundBase;

// ---------------------------------------------------------------------------
// UExplorationEnemySignalComponent
//
// The presentation half of an enemy's awareness. The player reads what an
// enemy is doing only from its body and its voice, so every state change the
// controller makes is turned into a montage and a sound here, from the
// type's UExplorationEnemyDefinition signals.
//
// It listens to AExplorationEnemyCharacter::OnStateChanged, OnDoorSignal and
// OnEatingChanged, and never drives the state machine. What it emits that
// the AI reacts to (the chase scream, knocking at a door, breaking it) goes
// through
// UExcursionNoiseSubsystem like any other noise (a footstep notify emits its
// noise the same way). The door noises sound at the door, with the door as
// their source.
//
// Two sounds loop: the investigation sound for as long as Investigate lasts,
// and the eat sound for as long as the enemy eats a scent bait (never at the
// same time). A loop is restarted whenever it ends so a one-shot asset still
// loops, and retried every half second when the engine did not start it (a
// short sound out of earshot is skipped, not queued). A paused world pauses
// it with every other 3D sound; a player capture freezes the enemy through
// CustomTimeDilation instead, so the loop is paused here while the owner's
// time is frozen. Ticks only while a loop is wanted. The eat montage plays
// when eating starts and is stopped when it ends.
//
// Empty signal fields play nothing and log only at Verbose: the assets are
// authored after the code.
// ---------------------------------------------------------------------------
UCLASS(ClassGroup = (Nevergone), meta = (BlueprintSpawnableComponent))
class NEVERGONE_API UExplorationEnemySignalComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UExplorationEnemySignalComponent();

	// Which signal a transition plays. Pure, so it can be tested without a
	// world.
	static EExplorationEnemySignalMoment GetSignalMoment(EExplorationEnemyState OldState,
		EExplorationEnemyState NewState);

	// Which signal a change of the eating flag plays: Eat when it starts,
	// None when it stops (the loop and the montage stop). Pure, like
	// GetSignalMoment.
	static EExplorationEnemySignalMoment GetEatingSignalMoment(bool bEating);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	void HandleStateChanged(EExplorationEnemyState OldState, EExplorationEnemyState NewState);

	void HandleDoorSignal(EExplorationEnemySignalMoment Moment, ADoor* Door);

	void HandleEatingChanged(bool bEating);

	// Plays the montage and emits the noise at the door.
	void PlayDoorSignal(const FExplorationEnemyNoiseSignal& Signal, ADoor* Door, const TCHAR* MomentName);

	// Plays the montage and the one-shot sound of a signal.
	void PlaySignal(const FExplorationEnemySignal& Signal, const TCHAR* MomentName);

	void PlayChaseSignal(const FExplorationEnemyNoiseSignal& Signal);

	void PlayMontage(UAnimMontage* Montage, const TCHAR* MomentName);

	// Loops Sound until StopSignalLoop, replacing any loop playing.
	// MomentName is for the log only and must be a literal.
	void StartSignalLoop(USoundBase* Sound, const TCHAR* MomentName);

	void StopSignalLoop();

	// Spawns the loop sound again. Called on start and whenever it ended
	// while the loop is still wanted.
	void SpawnLoopSound();

	AExplorationEnemyCharacter* GetEnemy() const;

	const UExplorationEnemyDefinition* GetDefinition() const;

	UPROPERTY()
	TObjectPtr<UAudioComponent> SignalLoop = nullptr;

	// The sound looping, or null when no loop is wanted.
	UPROPERTY()
	TObjectPtr<USoundBase> LoopSound = nullptr;

	const TCHAR* LoopName = TEXT("");

	// Countdown before the next attempt after the loop failed to start.
	float LoopRetrySeconds = 0.0f;

	FDelegateHandle StateChangedHandle;

	FDelegateHandle DoorSignalHandle;

	FDelegateHandle EatingChangedHandle;
};
