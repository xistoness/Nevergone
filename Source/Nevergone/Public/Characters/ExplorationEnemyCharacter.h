// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Types/ExplorationEnemyTypes.h"
#include "ExplorationEnemyCharacter.generated.h"

class ADoor;
class UExplorationEnemyDefinition;
class UExplorationEnemySignalComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnExplorationEnemyStateChanged,
	EExplorationEnemyState /*OldState*/, EExplorationEnemyState /*NewState*/);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnExplorationEnemyDoorSignal,
	EExplorationEnemySignalMoment /*Moment*/, ADoor* /*Door*/);

// ---------------------------------------------------------------------------
// AExplorationEnemyCharacter
//
// The body of a daytime enemy. Holds no decision logic: it applies its
// UExplorationEnemyDefinition, resolves one visual variant, exposes its
// behaviour state, and answers proximity questions. All decisions belong to
// AExplorationEnemyController; turning state changes into animation and sound
// belongs to UExplorationEnemySignalComponent.
//
// Deliberately derives from ACharacter and NOT from ACharacterBase: that class
// carries a spring arm and camera, the exploration/battle mode components, an
// AbilitySystemComponent, UnitStatsComponent and the selection decal — all of
// it party-member infrastructure that a daytime enemy never uses. Nighttime
// enemies remain AEnemyUnitBase; the two hierarchies stay separate on purpose.
//
// Usage:
//  1. Create a Blueprint subclass (art only, no logic).
//  2. Assign a UExplorationEnemyDefinition.
//  3. For Route patrol, fill PatrolWaypoints on the placed instance.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API AExplorationEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	AExplorationEnemyCharacter();

	// Broadcast whenever the controller moves this enemy between states.
	// The encounter flow listens for the transition into Catch.
	FOnExplorationEnemyStateChanged OnStateChanged;

	// Broadcast when the controller knocks at a door or breaks it. Played by
	// UExplorationEnemySignalComponent, like the state signals.
	FOnExplorationEnemyDoorSignal OnDoorSignal;

	// Moment is DoorKnock or DoorBreak.
	void AnnounceDoorSignal(EExplorationEnemySignalMoment Moment, ADoor* Door);

	// --- Definition ---

	UExplorationEnemyDefinition* GetDefinition() const { return Definition; }

	// --- Behaviour state ---

	EExplorationEnemyState GetBehaviorState() const { return BehaviorState; }

	// Sets the state, applies the matching movement speed, and broadcasts.
	// No-op when the state is unchanged.
	void SetBehaviorState(EExplorationEnemyState NewState);

	// --- Queries used by the controller ---

	// Location the actor occupied on spawn. Anchor for Stationary and Wander,
	// and the point a giving-up enemy returns to.
	const FVector& GetHomeLocation() const { return HomeLocation; }

	const TArray<TObjectPtr<AActor>>& GetPatrolWaypoints() const { return PatrolWaypoints; }

	// Effective patrol mode, downgraded to Stationary when Route is configured
	// without at least two usable waypoints.
	EExplorationPatrolMode GetEffectivePatrolMode() const;

	float GetCatchDistance() const;

	// Horizontal distance check against CatchDistance. Horizontal on purpose:
	// a survivor one floor up is not caught, and a capsule height difference
	// on a ramp should not block a catch that visually happened.
	bool IsWithinCatchDistance(const AActor* Target) const;

protected:

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	// The enemy type. Every tuning value comes from here.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UExplorationEnemyDefinition> Definition = nullptr;

	// Ordered patrol route, authored per placed instance. Only read when the
	// definition uses EExplorationPatrolMode::Route.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Enemy|Patrol")
	TArray<TObjectPtr<AActor>> PatrolWaypoints;

	// Plays the montage and sound of each state change.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UExplorationEnemySignalComponent> SignalComponent = nullptr;

private:

	// Applies collision, movement and visuals from the definition.
	void ApplyDefinition();

	// Loads and applies one variant. Soft references are resolved synchronously
	// here: this runs once per actor at spawn, and an async load would leave
	// the enemy invisible for the first frames of an excursion.
	void ApplyVisualVariant();

	// Resolves which variant this actor uses.
	//
	// Derived from the actor's FName rather than randomised, so the same enemy
	// keeps the same body across save/load and PIE sessions — a runtime
	// FMath::RandRange would reshuffle every zombie on every load. Level-placed
	// actors have an FName serialised in the level asset, which is exactly the
	// P0 case. When procedural buildings arrive (P3) enemies will be spawned at
	// runtime and their names stop being stable: the seed must move to a value
	// persisted through USaveableComponent at that point.
	int32 ResolveVisualVariantIndex(int32 VariantCount) const;

	EExplorationEnemyState BehaviorState = EExplorationEnemyState::Idle;

	FVector HomeLocation = FVector::ZeroVector;

	int32 ActiveVisualVariantIndex = INDEX_NONE;
};