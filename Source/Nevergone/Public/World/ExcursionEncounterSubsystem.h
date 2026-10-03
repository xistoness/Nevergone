// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ExcursionEncounterSubsystem.generated.h"

class ACharacter;
class AExcursionSurvivorNPC;
class AExplorationEnemyCharacter;
class AExplorationPlayerController;
class UExplorationEnemyDefinition;
class USkeletalMeshComponent;
struct FExcursionCaptureAnimation;

// How a capture ended for its victim. Emitted once per capture.
UENUM(BlueprintType)
enum class EExcursionCaptureOutcome : uint8
{
	// The survivor used a safeguard and killed the enemy.
	Unharmed,

	// Bitten, pushed the enemy down and got away.
	Infected,

	// The victim died: the player's survivor or a follower, devoured.
	Killed,
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCaptureResolved,
	ACharacter* /*Victim*/, AExplorationEnemyCharacter* /*Enemy*/, EExcursionCaptureOutcome /*Outcome*/);

// One capture in progress. Plain struct, never reflected: it only lives
// inside the subsystem.
struct FExcursionCaptureSequence
{
	TWeakObjectPtr<AExplorationEnemyCharacter> Enemy;

	TWeakObjectPtr<ACharacter> Victim;

	EExcursionCaptureOutcome Outcome = EExcursionCaptureOutcome::Killed;

	// Counted in the enemy's own time, so a devour frozen by a player capture
	// waits for it instead of finishing behind the player's back.
	float TimeLeft = 0.0f;

	bool bVictimIsPlayer = false;
};

// ---------------------------------------------------------------------------
// UExcursionEncounterSubsystem
//
// Resolves what happens when a daytime enemy catches a survivor. There is no
// choice screen: the outcome is decided the moment the catch is reported.
//
//   Follower caught   Dead on the spot, devoured for the type's
//                     DevourFollowerSeconds while the world keeps running.
//                     The enemy then goes to the last place it sensed the
//                     player.
//   Player caught     Everyone else freezes. With a safeguard in the
//                     inventory it is used and broken, and the enemy dies.
//                     Without one, a roll picks bite or death:
//                       Bite   the enemy is knocked down, then chases again;
//                              the survivor is infected.
//                       Death  the survivor is devoured and every recruited
//                              follower dies too. The world stays frozen:
//                              the excursion is over.
//
// Freezing uses each actor's CustomTimeDilation, not SetPause: a paused
// world mutes every 3D sound, the capture's own included. Perception keeps
// running for frozen enemies, so they can hear the struggle and react once
// the freeze ends.
//
// Death chance, in percent:
//   BaseDeathChance (enemy type)
//   - Strength x DeathChanceReductionPerStrength
//   + earlier bites this excursion x DeathChanceBonusPerPriorBite
// clamped to 0-100. The two constants live in UExcursionEncounterSettings.
//
// Outcomes are broadcast through OnCaptureResolved. The subsystem lives and
// dies with the excursion level, and so does everything it counts.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UExcursionEncounterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	static UExcursionEncounterSubsystem* Get(const UObject* WorldContextObject);

	// Called by an enemy controller when its target is within catch distance.
	// False when the capture is refused (the enemy or the victim is already
	// in one, or the player is dead); the enemy keeps what it was doing.
	bool ReportCatch(AExplorationEnemyCharacter* Enemy, ACharacter* Victim);

	FOnCaptureResolved OnCaptureResolved;

	// The death chance formula from the class comment, in percent, clamped
	// to 0-100. Pure, so it can be tested without a world.
	static float CalculateDeathChance(float BaseDeathChance, int32 Strength, int32 PriorBites,
		float ReductionPerStrength, float BonusPerPriorBite);

	// --- UTickableWorldSubsystem ---

	virtual void Tick(float DeltaTime) override;

	virtual TStatId GetStatId() const override;

protected:

	// Game and PIE only, like the noise subsystem.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:

	void StartFollowerCapture(AExplorationEnemyCharacter* Enemy, AExcursionSurvivorNPC* Follower,
		const UExplorationEnemyDefinition& Definition);

	void StartPlayerCapture(AExplorationEnemyCharacter* Enemy, ACharacter* Player,
		const UExplorationEnemyDefinition& Definition);

	void FinishCapture(const FExcursionCaptureSequence& Capture);

	bool IsInCapture(const AActor* Actor) const;

	// Removes one item tagged Item.Safeguard from the party inventory.
	bool TryConsumeSafeguard() const;

	float ComputeDeathChance(const UExplorationEnemyDefinition& Definition, ACharacter* Survivor) const;

	static int32 GetStrength(const ACharacter* Survivor);

	// Places the enemy StandOffDistance in front of the victim, the two
	// facing each other, as the paired animations were authored.
	static void AlignForCapture(AExplorationEnemyCharacter* Enemy, ACharacter* Victim, float StandOffDistance);

	// Starts both montages on the same frame. Returns the longer length, or
	// zero when the pair has no montage.
	static float PlayPairedAnimation(AExplorationEnemyCharacter* Enemy, ACharacter* Victim,
		const FExcursionCaptureAnimation& Animation);

	// Stops the body and hands it to physics as a ragdoll, watched by
	// WatchRagdolls until it settles.
	void BecomeCorpse(ACharacter* Character);

	// A ragdoll whose physics asset blows up produces NaN transforms, and
	// NaN bodies corrupt the physics scene: every query nearby (the
	// player's movement sweeps) fails from then on. A ragdoll that goes NaN
	// is taken out of physics and hidden, with an error naming the asset;
	// one that settles stops being watched.
	void WatchRagdolls();

	void KillRecruitedSurvivors();

	void FreezeWorldExcept(const AActor* KeepA, const AActor* KeepB);

	void UnfreezeWorld();

	static AExplorationPlayerController* GetPlayerController(const ACharacter* Player);

	TArray<FExcursionCaptureSequence> Captures;

	TMap<TWeakObjectPtr<AActor>, int32> BitesBySurvivor;

	// Actors whose time dilation the current player capture set to zero.
	TArray<TWeakObjectPtr<AActor>> FrozenActors;

	// Ragdolls still moving.
	TArray<TWeakObjectPtr<USkeletalMeshComponent>> ActiveRagdolls;

	bool bPlayerDied = false;
};
