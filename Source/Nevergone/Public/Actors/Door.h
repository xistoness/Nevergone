// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "Interfaces/SaveParticipant.h"
#include "Types/DoorTypes.h"
#include "Door.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USphereComponent;
class UBoxComponent;
class UInteractableComponent;
class USaveableComponent;
class UItemDefinition;
class UNoiseDefinition;
class UNavLinkCustomComponent;
class UPathFollowingComponent;

// ---------------------------------------------------------------------------
// ADoor
//
// Placed actor that represents a door in the exploration mode, crossable by
// the player and by AI (enemies and the follower).
// Implements:
//   - IInteractable    — the player's interact: unlock with the key, open,
//                        close; held on the latch side, the latch.
//   - ISaveParticipant — persists the bIsUnlocked flag via SaveableComponent.
//
// State machine (the door stays as it was left):
//
//    +-------+ has key  +--------+   open    +------+
//    |Locked | -------> | Closed | <-------> | Open |
//    +-------+   (key   +--------+   close   +------+
//        |     consumed)  ^    |   (jolts while somebody
//        |                |    |    stands in the doorway)
//        +-- no key:      |    | hold, latch side (an open door closes first)
//            locked       |    v
//            feedback   +---------+  enemy knocks   +--------+
//                       | Latched | --------------> | Broken |
//                       +---------+  BreakSeconds   +--------+
//
//   Open never re-locks; bIsUnlocked is one-way. Latched opens from the
//   latch side only (a tap unlatches and opens, a hold only unlatches);
//   from the other side it jolts. Broken stays open for the rest of the
//   excursion. Neither the latch nor a broken door is saved: the excursion
//   never saves halfway.
//
// Player and AI share TryOpen / TryClose. The player reaches them through
// Interact, which is also the only path that spends the party's key and
// works the latch (DoorRules::ResolveInteraction). AI reaches them through
// the nav link and never calls Interact.
//
// AI navigation: the navmesh stays static and the collision box carves the
// doorway out of it. DoorNavLink is a smart link across the carve. When an
// agent's path reaches it, DoorRules::ResolveAgentPassage decides by state:
// walk through (Open, Broken), stop, open after AgentOpenDelaySeconds and
// walk through (Closed), stop and wait (Latched: the controller hears it
// through IDoorAgent, and an enemy knocks until TryBreak), or never offered
// to pathfinding (Locked, link off).
//
// The leaf swing is C++: DoorMesh yaws OpenAngleDegrees around its pivot
// (the hinge) over SwingDurationSeconds, always away from whoever opened
// it. The leaf only answers queries (interact trace, sight): the open leaf
// is what the player interacts with to close it. BP_OnDoorOpening
// / BP_OnDoorClosing stay as hooks for extra effects, not for the swing.
// A jolt (closing on somebody, pushing a latched door) is a short offset on
// top of the swing, JoltAngleDegrees out and back in JoltDurationSeconds,
// with no collision of its own.
//
// Every door noise is emitted with the door as its source: the door never
// counts as a wall for its own noise.
//
// Component layout:
//   Root (USceneComponent)
//   ├── DoorMesh          (UStaticMeshComponent — the swinging leaf, QueryOnly)
//   ├── DoorCollisionBox  (UBoxComponent        — solid blocker, covers the whole doorway)
//   ├── InteractionSphere (USphereComponent     — QueryOnly trace volume)
//   └── DoorNavLink       (UNavLinkCustomComponent — not a scene component)
//
// The box is what seals the doorway: a leaf mesh without collision (or with
// the wrong one) never lets a pawn through. While closed, the box also blocks
// enemy sight and counts as a wall for hearing occlusion; while open it has
// no collision at all; the swung-aside leaf blocks only sight and the
// interact trace.
//
// The SceneComponent root keeps the actor pivot independent from the mesh
// pivot — the swing rotates DoorMesh around its hinge without dragging the
// box or the sphere along. The actor pivot need not sit on the floor: the
// nav link ends find the floor themselves.
// ---------------------------------------------------------------------------
UCLASS(Blueprintable)
class NEVERGONE_API ADoor : public AActor,
                            public IInteractable,
                            public ISaveParticipant
{
    GENERATED_BODY()

public:

    ADoor();

    // -----------------------------------------------------------------------
    // IInteractable
    // -----------------------------------------------------------------------
    virtual void Interact_Implementation(AActor* Interactor) override;

    // Holding interact works the latch, from the latch side only.
    virtual bool CanInteractHold(const AActor* Interactor) const override;
    virtual void InteractHold(AActor* Interactor) override;

    // -----------------------------------------------------------------------
    // ISaveParticipant
    // -----------------------------------------------------------------------
    virtual void WriteSaveData_Implementation(FActorSaveData& OutData) const override;
    virtual void ReadSaveData_Implementation(const FActorSaveData& InData) override;
    virtual void OnPostRestore_Implementation() override;

    // -----------------------------------------------------------------------
    // Open / close — shared by the player (through Interact) and AI
    // -----------------------------------------------------------------------

    // Opens a Closed door. The noise is attributed to Opener. True when the
    // door is open afterwards (already open or broken counts); false while
    // Locked or Latched. Never touches the key or the latch: that is
    // Interact's job.
    bool TryOpen(AActor* Opener);

    // Closes an Open door. While a pawn stands in the doorway the leaf jolts
    // instead (a noise attributed to Closer) and it fails. True when the
    // door is closed afterwards (already closed counts); false when broken.
    bool TryClose(AActor* Closer);

    // A latched door gives way to Breaker: Broken, open for good, every
    // waiting agent walks through. False unless Latched.
    bool TryBreak(AActor* Breaker);

    // True while a pawn's capsule overlaps the box (the doorway), or an
    // agent is walking the nav link through it.
    bool IsDoorwayOccupied() const;

    // True while this agent's move is paused at the door, waiting for it.
    bool IsAgentWaiting(const UPathFollowingComponent* PathFollowing) const;

    // True while Pawn stands between the two ends of the nav link, inside
    // the doorway carved out of the navmesh.
    bool IsInDoorway(const AActor* Pawn) const;

    // Where every door noise sounds: the centre of the box (the doorway).
    FVector GetNoiseLocation() const;

    // How long an enemy knocks at the latched door before it breaks.
    float GetBreakSeconds() const { return BreakSeconds; }

    // -----------------------------------------------------------------------
    // Read access
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintPure, Category = "Door")
    EDoorState GetState() const { return CurrentState; }

    UFUNCTION(BlueprintPure, Category = "Door")
    bool IsLocked() const { return CurrentState == EDoorState::Locked; }

    UFUNCTION(BlueprintPure, Category = "Door")
    bool IsUnlocked() const { return bIsUnlocked; }

    UFUNCTION(BlueprintPure, Category = "Door")
    bool IsLatched() const { return CurrentState == EDoorState::Latched; }

protected:

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

    // -----------------------------------------------------------------------
    // Configuration — designer-tweakable per placement
    // -----------------------------------------------------------------------

    // If true, the door starts in the Locked state on a fresh save.
    // If false, it starts in Closed (already unlocked).
    // Once unlocked, this field is irrelevant — bIsUnlocked drives the post-load state.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Config")
    bool bStartLocked = true;

    // Item required to unlock this door. Consumed (one-time use) on success.
    // If null AND bStartLocked is true, a warning is logged at BeginPlay:
    // nothing can unlock the door.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Config")
    TSoftObjectPtr<UItemDefinition> RequiredKey;

    // Message shown to the player when interacting with a Locked door without
    // the right key. Falls back to a generic "It's locked." if left empty.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Config")
    FText LockedPromptText;

    // Side of the doorway the latch is on, set per placed door. From that
    // side, holding interact puts the latch on or takes it off; from the
    // other side a latched door does not open for anybody.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Config")
    EDoorLatchSide LatchSide = EDoorLatchSide::None;

    // -----------------------------------------------------------------------
    // AI
    // -----------------------------------------------------------------------

    // How long an agent (enemy or follower) stands at a closed door before
    // it opens and walks through.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|AI", meta = (ClampMin = "0.0", Units = "s"))
    float AgentOpenDelaySeconds = 0.6f;

    // How long an enemy knocks at the latched door before it breaks it,
    // counted in the enemy's own time. The knock rhythm belongs to the
    // enemy type (KnockIntervalSeconds).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|AI", meta = (ClampMin = "0.0", Units = "s"))
    float BreakSeconds = 5.f;

    // Distance from the door plane to each end of the nav link. Both ends
    // must land on navmesh: past the carve of the box (half its thickness
    // plus the agent radius, 42). Changing it requires rebuilding paths.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|AI", meta = (ClampMin = "0.0", Units = "cm"))
    float NavLinkHalfLength = 80.f;

    // -----------------------------------------------------------------------
    // Swing
    // -----------------------------------------------------------------------

    // How far the leaf turns around its pivot when open.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Swing", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
    float OpenAngleDegrees = 90.f;

    // Time for a full swing, open or closed.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Swing", meta = (ClampMin = "0.0", Units = "s"))
    float SwingDurationSeconds = 0.5f;

    // How far the leaf moves before it catches, when closed on somebody in
    // the doorway or pushed while latched.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Swing", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "deg"))
    float JoltAngleDegrees = 8.f;

    // Time for a whole jolt, out and back.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Swing", meta = (ClampMin = "0.0", Units = "s"))
    float JoltDurationSeconds = 0.3f;

    // -----------------------------------------------------------------------
    // Noises — sound for the player and noise for enemies, one asset each.
    // Authored on the Blueprint subclass, so a squeaky door and a quiet one
    // are different subclasses (or per-instance overrides). A null entry is
    // silent to both. Attributed to whoever moved the door: enemies hunt
    // the noise of a survivor body only, so an enemy opening a door is heard
    // by the player and by no other enemy. Knocking and breaking the door
    // are the enemy's own noises (its type's signals).
    // -----------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Noise")
    TObjectPtr<UNoiseDefinition> LockedAttemptNoise;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Noise")
    TObjectPtr<UNoiseDefinition> UnlockNoise;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Noise")
    TObjectPtr<UNoiseDefinition> OpenNoise;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Noise")
    TObjectPtr<UNoiseDefinition> CloseNoise;

    // The leaf catching: closed on somebody in the doorway, or pushed while
    // latched.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Noise")
    TObjectPtr<UNoiseDefinition> JoltNoise;

    // -----------------------------------------------------------------------
    // Components
    // -----------------------------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> Root;

    // Door leaf, swung by C++. QueryOnly: blocks Visibility and Interactable,
    // ignores pawns, camera and hearing occlusion; never shapes the navmesh.
    // BP subclass assigns the StaticMesh asset (with simple collision), with
    // its pivot on the hinge.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> DoorMesh;

    // Solid collider that does the actual pawn blocking. Must cover the whole
    // doorway; the C++ default is 120 wide, for a clear opening of at least
    // 110. Thickness along local X, which is also the axis the nav link
    // crosses. Toggled on/off by ApplyState.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> DoorCollisionBox;

    // Wider trigger volume for the interaction line trace. QueryOnly — never
    // physically obstructs anything, only returns hits to the trace. A trace
    // that starts inside it misses it: up close, the leaf is the target.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;

    // Smart link across the doorway, for AI pathfinding. Ends placed by
    // OnConstruction from the box and NavLinkHalfLength.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UNavLinkCustomComponent> DoorNavLink;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UInteractableComponent> InteractableComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USaveableComponent> SaveableComponent;

    // -----------------------------------------------------------------------
    // Blueprint hooks — implement in BP subclass to drive visuals
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintImplementableEvent, Category = "Door|Visuals")
    void BP_OnDoorOpening();

    UFUNCTION(BlueprintImplementableEvent, Category = "Door|Visuals")
    void BP_OnDoorClosing();

    UFUNCTION(BlueprintImplementableEvent, Category = "Door|Visuals")
    void BP_OnUnlocked();

    UFUNCTION(BlueprintImplementableEvent, Category = "Door|Visuals")
    void BP_OnLockedAttempt();

    UFUNCTION(BlueprintImplementableEvent, Category = "Door|Visuals")
    void BP_OnDoorBroken();

private:

    // -----------------------------------------------------------------------
    // Runtime state — never directly editable in the editor
    // -----------------------------------------------------------------------

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Door|State", meta = (AllowPrivateAccess = "true"))
    EDoorState CurrentState = EDoorState::Locked;

    UPROPERTY()
    bool bIsUnlocked = false;

    static const FName SaveKey;

    // -----------------------------------------------------------------------
    // Internal — state transitions
    // -----------------------------------------------------------------------

    // Carries out what the player's interact resolved to.
    void ApplyInteraction(EDoorInteraction Interaction, AActor* Interactor);

    void HandleKeyInteraction(AActor* Interactor);
    void HandleUnlockAndOpen(AActor* Interactor);
    void HandleLockedAttempt(AActor* Interactor);

    // Sets the state and applies everything that follows from it, including
    // what agents already waiting at the door do now.
    void SetState(EDoorState NewState);

    // Applies the state to the components:
    // - Locked / Closed / Latched: box blocks; sphere accepts the interact trace.
    // - Open: box has no collision; sphere still accepts the trace.
    // - Broken: box and sphere have no collision, and the leaf stops taking
    //   the interact trace: nothing is left to interact with.
    // - Nav link on unless DoorRules says the door is a wall for AI.
    //
    // DoorMesh collision is otherwise never touched here: the leaf keeps its own.
    void ApplyState();

    // The box's thickness axis in actor space, flat: what the nav link
    // crosses and the leaf swings towards.
    FVector GetDoorwayAxis() const;

    // Signed distance of Actor from the doorway along GetDoorwayAxis.
    float GetSideOf(const AActor* Actor) const;

    bool IsOnLatchSide(const AActor* Actor) const;

    // Places the nav link ends across the box, each on the floor below it.
    void PlaceNavLinkEnds();

    // -----------------------------------------------------------------------
    // Leaf swing
    // -----------------------------------------------------------------------

    // Sets the leaf's query-only collision (see DoorMesh), overriding any
    // value saved on the Blueprint or the placed instance.
    void ApplyLeafCollision();

    // Records the authored leaf rotation as the closed pose.
    void CaptureClosedLeaf();

    // +1 or -1: the swing direction that takes the leaf to the side of the
    // doorway opposite Opener. +1 when it cannot tell.
    float ChooseSwingSign(const AActor* Opener) const;

    // Swings the leaf towards NewTargetYaw (0 is closed), ticking until there.
    void StartSwing(float NewTargetYaw);

    // A jolt towards Sign (+1 or -1 yaw) on top of the current swing, with
    // the jolt noise attributed to JoltInstigator.
    void StartJolt(float Sign, AActor* JoltInstigator);

    void ApplyLeafRotation();

    // Closed leaf pose, relative to the root.
    FQuat ClosedLeafRotation = FQuat::Identity;

    // Current and target yaw of the leaf away from the closed pose.
    float SwingYaw = 0.f;
    float TargetSwingYaw = 0.f;

    // The jolt in progress: direction and time into it.
    bool bJolting = false;
    float JoltSign = 1.f;
    float JoltElapsed = 0.f;

    // Emits one of the door noises at GetNoiseLocation, attributed to
    // NoiseInstigator, with the door as its source.
    void EmitDoorNoise(const UNoiseDefinition* Noise, const TCHAR* DebugLabel, AActor* NoiseInstigator) const;

    // -----------------------------------------------------------------------
    // AI passage
    // -----------------------------------------------------------------------

    // Bound to DoorNavLink: an agent's path just reached the link.
    void HandleAgentReachedLink(UNavLinkCustomComponent* Link, UObject* PathComp, const FVector& DestPoint);

    // AgentOpenDelaySeconds after the first agent stopped at the closed door.
    void HandleAgentOpenDelayElapsed();

    // Lets every waiting agent walk through and clears the open delay.
    void ReleaseWaitingAgents();

    // After a state change: a Closed door starts the open delay for agents
    // already waiting; a Latched one stops it and tells them it is barred.
    void UpdateWaitingAgents();

    void StartAgentOpenTimer();

    // Tells the agent's controller, if it is an IDoorAgent, that the door is barred.
    void NotifyAgentBarred(UPathFollowingComponent* PathFollowing);

    // An agent at the link, the move request it had there and the link end
    // it is heading to. A new request (the agent changed its mind) makes the
    // entry stale.
    struct FAgentMove
    {
        TWeakObjectPtr<UPathFollowingComponent> PathFollowing;
        FAIRequestID RequestId;
        FVector Exit = FVector::ZeroVector;
    };

    // True while the agent is still paused on the request it had at the door.
    static bool IsStillWaiting(const FAgentMove& Agent);

    // Records an agent that walks the link now, dropping stale entries, and
    // tells its controller, if it is an IDoorAgent, where it comes out.
    void AddCrossingAgent(const FAgentMove& Agent);

    // True while the agent is still on the request it crossed with and has
    // not yet cleared the doorway (see IsDoorwayOccupied).
    bool IsStillCrossing(const FAgentMove& Agent) const;

    static AActor* GetAgentPawn(const UPathFollowingComponent* PathFollowing);

    // Agents paused at the closed or latched door, waiting for it to open.
    TArray<FAgentMove> WaitingAgents;

    // Agents released through the link, until they clear the doorway.
    TArray<FAgentMove> CrossingAgents;

    FTimerHandle AgentOpenTimerHandle;
};
