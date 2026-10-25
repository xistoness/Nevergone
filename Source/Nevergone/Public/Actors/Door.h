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
//   - IInteractable    — the player's interact: unlock with the key, open, close.
//   - ISaveParticipant — persists the bIsUnlocked flag via SaveableComponent.
//
// State machine (the door stays as it was left):
//
//                +-------+ has key  +--------+   open    +------+
//                |Locked | -------> | Closed | <-------> | Open |
//                +-------+   (key   +--------+   close   +------+
//                    |     consumed)              (fails while somebody
//                    +-- no key: locked feedback   stands in the doorway)
//
//   Open never re-locks; bIsUnlocked is one-way.
//
// Player and AI share TryOpen / TryClose. The player reaches them through
// Interact, which is also the only path that spends the party's key. AI
// reaches them through the nav link and never calls Interact.
//
// AI navigation: the navmesh stays static and the collision box carves the
// doorway out of it. DoorNavLink is a smart link across the carve. When an
// agent's path reaches it, DoorRules::ResolveAgentPassage decides by state:
// walk through (Open), stop, open after AgentOpenDelaySeconds and walk
// through (Closed), or never offered to pathfinding (Locked, link off).
//
// The leaf swing is C++: DoorMesh yaws OpenAngleDegrees around its pivot
// (the hinge) over SwingDurationSeconds, always away from whoever opened
// it, so it never swings into them. The leaf never collides. BP_OnDoorOpening
// / BP_OnDoorClosing stay as hooks for extra effects, not for the swing.
//
// Component layout:
//   Root (USceneComponent)
//   ├── DoorMesh          (UStaticMeshComponent — VISUAL ONLY, NoCollision)
//   ├── DoorCollisionBox  (UBoxComponent        — solid blocker, covers the whole doorway)
//   ├── InteractionSphere (USphereComponent     — QueryOnly trace volume)
//   └── DoorNavLink       (UNavLinkCustomComponent — not a scene component)
//
// The box is the actual collider — the mesh is purely cosmetic, so a mesh
// without collision (or with the wrong one) never lets a pawn through. While
// closed, the box also blocks enemy sight and counts as a wall for hearing
// occlusion; while open it has no collision at all.
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
    // door is open afterwards (already open counts); false while Locked.
    // Never touches the key: unlocking is Interact's job.
    bool TryOpen(AActor* Opener);

    // Closes an Open door. Fails while a pawn stands in the doorway. True
    // when the door is closed afterwards (already closed counts).
    bool TryClose(AActor* Closer);

    // True while a pawn's capsule overlaps the leaf, or an agent is walking
    // the nav link through the doorway.
    bool IsDoorwayOccupied() const;

    // -----------------------------------------------------------------------
    // Read access
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintPure, Category = "Door")
    EDoorState GetState() const { return CurrentState; }

    UFUNCTION(BlueprintPure, Category = "Door")
    bool IsLocked() const { return CurrentState == EDoorState::Locked; }

    UFUNCTION(BlueprintPure, Category = "Door")
    bool IsUnlocked() const { return bIsUnlocked; }

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

    // -----------------------------------------------------------------------
    // AI
    // -----------------------------------------------------------------------

    // How long an agent (enemy or follower) stands at a closed door before
    // it opens and walks through.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|AI", meta = (ClampMin = "0.0", Units = "s"))
    float AgentOpenDelaySeconds = 0.6f;

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

    // -----------------------------------------------------------------------
    // Noises — sound for the player and noise for enemies, one asset each.
    // Authored on the Blueprint subclass, so a squeaky door and a quiet one
    // are different subclasses (or per-instance overrides). A null entry is
    // silent to both. Attributed to whoever moved the door: enemies hunt
    // the noise of a survivor body only, so an enemy opening a door is heard
    // by the player and by no other enemy.
    // -----------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Noise")
    TObjectPtr<UNoiseDefinition> LockedAttemptNoise;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Noise")
    TObjectPtr<UNoiseDefinition> UnlockNoise;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Noise")
    TObjectPtr<UNoiseDefinition> OpenNoise;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Noise")
    TObjectPtr<UNoiseDefinition> CloseNoise;

    // -----------------------------------------------------------------------
    // Components
    // -----------------------------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> Root;

    // Visual-only door leaf, swung by C++. Never collides, whatever the
    // Blueprint sets (EnforceLeafWithoutCollision). BP subclass assigns the
    // StaticMesh asset, with its pivot on the hinge.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> DoorMesh;

    // Solid collider that does the actual pawn blocking. Must cover the whole
    // doorway; the C++ default is 120 wide, for a clear opening of at least
    // 110. Thickness along local X, which is also the axis the nav link
    // crosses. Toggled on/off by ApplyState.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> DoorCollisionBox;

    // Wider trigger volume for the interaction line trace. QueryOnly — never
    // physically obstructs anything, only returns hits to the trace. Stays
    // active while open so the player can close the door.
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

    void HandleUnlockAndOpen(AActor* Interactor);
    void HandleLockedAttempt(AActor* Interactor);

    // Sets the state and applies everything that follows from it.
    void SetState(EDoorState NewState);

    // Applies the state to the components:
    // - Locked / Closed: box blocks; sphere accepts the interact trace.
    // - Open: box has no collision; sphere still accepts the trace.
    // - Nav link on unless DoorRules says the door is a wall for AI.
    //
    // DoorMesh is never touched here — it stays NoCollision for its entire lifetime.
    void ApplyState();

    // The box's thickness axis in actor space, flat: what the nav link
    // crosses and the leaf swings towards.
    FVector GetDoorwayAxis() const;

    // Places the nav link ends across the box, each on the floor below it.
    void PlaceNavLinkEnds();

    // -----------------------------------------------------------------------
    // Leaf swing
    // -----------------------------------------------------------------------

    // Turns off any collision the Blueprint gave DoorMesh.
    void EnforceLeafWithoutCollision();

    // Records the authored leaf rotation as the closed pose.
    void CaptureClosedLeaf();

    // +1 or -1: the swing direction that takes the leaf to the side of the
    // doorway opposite Opener. +1 when it cannot tell.
    float ChooseSwingSign(const AActor* Opener) const;

    // Swings the leaf towards NewTargetYaw (0 is closed), ticking until there.
    void StartSwing(float NewTargetYaw);

    void ApplyLeafRotation();

    // Closed leaf pose, relative to the root.
    FQuat ClosedLeafRotation = FQuat::Identity;

    // Current and target yaw of the leaf away from the closed pose.
    float SwingYaw = 0.f;
    float TargetSwingYaw = 0.f;

    // Emits one of the door noises at the door leaf, attributed to NoiseInstigator.
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

    // An agent at the link, and the move request it had there. A new
    // request (the agent changed its mind) makes the entry stale.
    struct FAgentMove
    {
        TWeakObjectPtr<UPathFollowingComponent> PathFollowing;
        FAIRequestID RequestId;
    };

    // True while the agent is still paused on the request it had at the door.
    static bool IsStillWaiting(const FAgentMove& Agent);

    // Records an agent that walks the link now, dropping stale entries.
    void AddCrossingAgent(const FAgentMove& Agent);

    // True while the agent is still on the request it crossed with and has
    // not yet cleared the doorway (see IsDoorwayOccupied).
    bool IsStillCrossing(const FAgentMove& Agent) const;

    static AActor* GetAgentPawn(const UPathFollowingComponent* PathFollowing);

    // Agents paused at the closed door, waiting for it to open.
    TArray<FAgentMove> WaitingAgents;

    // Agents released through the link, until they clear the doorway.
    TArray<FAgentMove> CrossingAgents;

    FTimerHandle AgentOpenTimerHandle;
};
