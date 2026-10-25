// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionComponent.h"
#include "ExplorationPerceptionComponent.generated.h"

class UExplorationEnemyDefinition;

// Which sense produced a stimulus. The FSM treats them differently: hearing is
// only ever a reason to investigate, while sight and smell can escalate to a
// chase.
UENUM()
enum class EExplorationSenseChannel : uint8
{
	Sight,
	Hearing,
	Smell,
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSurvivorSensed,
	AActor* /*Survivor*/, const FVector& /*StimulusLocation*/, EExplorationSenseChannel /*Channel*/);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSurvivorLost, AActor* /*Survivor*/);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPlaceNoiseHeard,
	AActor* /*Source*/, const FVector& /*NoiseLocation*/);

// What one enemy currently knows about one survivor. Plain struct, never
// reflected: it only lives inside the perception component.
struct FExplorationSensedSurvivor
{
	FVector LastKnownLocation = FVector::ZeroVector;

	bool bHasLastKnownLocation = false;

	bool bSight = false;

	bool bSmell = false;
};

// ---------------------------------------------------------------------------
// UExplorationPerceptionComponent
//
// The three senses of a daytime enemy, configured entirely from the enemy's
// UExplorationEnemyDefinition. Lives on AExplorationEnemyController.
//
// Sight and hearing use the engine senses (AISense_Sight, AISense_Hearing) so
// the perception debugger, the sight cone gizmo and ReportNoiseEvent all work
// out of the box. Smell has no engine equivalent and is polled here as a plain
// radius check: no cone, no line of sight, no walls. It exists so that holding
// still right next to a type that has it is not a safe play.
//
// Hearing is also filtered here, per listener: the engine only checks
// distance against HearingRange x Loudness, so walls (occlusion) and
// ambient noise (mask volumes) are applied on top before a noise counts.
//
// A place noise (UExcursionNoiseSubsystem::IsHeardAsPlaceNoise: another
// enemy's chase scream, knocking at a door or breaking it) is heard whoever
// made it, through the same occlusion and masking, and reported on its own
// delegate: it points at a place, not at a survivor. An enemy never hears
// its own. Any other noise not made by a survivor body (an enemy opening a
// door) is ignored.
//
// Every survivor body is tracked separately: the survivor the player
// controls and any recruited follower (AExcursionSurvivorNPC::IsSurvivorBody).
// Sight is recorded for unrecruited survivors too and filtered when asked,
// because the engine does not fire a stable sight stimulus again: a survivor
// recruited while already in view would otherwise stay invisible.
//
// The component reports events and answers what it currently senses. It
// does not choose a target: that, escalation, memory and giving up all live
// in the controller's state machine.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UExplorationPerceptionComponent : public UAIPerceptionComponent
{
	GENERATED_BODY()

public:

	UExplorationPerceptionComponent();

	// Fired every time a sense produces a positive stimulus on a survivor
	// body, including repeats while it stays sensed.
	FOnSurvivorSensed OnSurvivorSensed;

	// Fired when a sense that was tracking a survivor body drops it. Sight
	// and smell report loss; hearing does not, because a noise is an instant.
	FOnSurvivorLost OnSurvivorLost;

	// Fired when a place noise is heard (see the class comment).
	// Not recorded as a stimulus: the listener decides whether it reacts,
	// and records it with SetMostRecentStimulus when it does.
	FOnPlaceNoiseHeard OnPlaceNoiseHeard;

	// Builds the sense configurations from the type. Must run before the
	// component is useful; the controller calls it on possess.
	void InitializeFromDefinition(const UExplorationEnemyDefinition* InDefinition);

	// True while sight currently holds this survivor body.
	bool HasSightOf(const AActor* Survivor) const;

	// True while sight or smell currently holds this survivor body.
	bool HasContactWith(const AActor* Survivor) const;

	// The closest survivor body sight currently holds, or null.
	AActor* GetClosestSightedSurvivor() const;

	// Last place any survivor body was sensed, by any channel: the most
	// recent stimulus. Valid after the survivor is lost, which is exactly
	// what Investigate needs.
	const FVector& GetLastKnownLocation() const { return LastKnownLocation; }

	bool HasLastKnownLocation() const { return bHasLastKnownLocation; }

	// Last place this survivor in particular was sensed, however long ago.
	// False only when the enemy never sensed them.
	bool GetLastKnownLocationOf(const AActor* Survivor, FVector& OutLocation) const;

	// Records where a survivor is. Used by the controller while the tracking
	// grace window is open, since that knowledge does not come from any sense.
	void SetLastKnownLocation(AActor* Survivor, const FVector& Location);

	// Makes a place the most recent stimulus without tying it to a survivor.
	// Used for a place noise: it points at a place, nobody was sensed there.
	void SetMostRecentStimulus(const FVector& Location);

	// Forgets the search once the enemy gives up: only the most recent
	// stimulus. Where each survivor was last sensed is kept for the whole
	// excursion, because an enemy that kills the follower goes to the last
	// place it sensed the player, however old.
	void ForgetLastKnownLocation();

protected:

	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:

	// Bound to the engine's perception delegate; routes sight and hearing into
	// the two delegates above.
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	// Polls the smell radius. Runs only when the type has bCanSmell.
	void UpdateSmell();

	// Stores the location on the survivor's own entry, and as the most recent
	// stimulus when the survivor is a body enemies hunt.
	void RecordStimulus(AActor* Survivor, const FVector& Location);

	// Applies occlusion and masking to a noise the engine already accepted.
	// True when the noise is still within reach after both.
	bool IsNoiseAudible(const AActor* Instigator, const FAIStimulus& Stimulus) const;

	// Number of occluding objects between the two points, capped. The
	// noise's Source (the door it came out of) is not counted, nor an object
	// touching the noise itself (the shelf a bottle fell from).
	int32 CountNoiseOccluders(const FVector& From, const FVector& To,
		const AActor* Listener, const AActor* Instigator, const AActor* Source) const;

	// The enemy body. The component lives on the controller.
	APawn* GetListenerPawn() const;

	const FExplorationSensedSurvivor* FindEntry(const AActor* Survivor) const;

	UPROPERTY()
	TObjectPtr<const UExplorationEnemyDefinition> Definition = nullptr;

	TMap<TWeakObjectPtr<AActor>, FExplorationSensedSurvivor> SensedSurvivors;

	FVector LastKnownLocation = FVector::ZeroVector;

	bool bHasLastKnownLocation = false;
};
