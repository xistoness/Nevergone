// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "Interfaces/SaveParticipant.h"
#include "Door.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class USphereComponent;
class UBoxComponent;
class USoundBase;
class UInteractableComponent;
class USaveableComponent;
class UItemDefinition;
class UAudioSubsystem;

// ---------------------------------------------------------------------------
// EDoorState
//
// Runtime-only state for a door. Only Locked / not-Locked is persisted
// (via FSaveableComponent + FActorSaveData). The Closed/Open distinction is
// transient: a door restored from save always materializes in Closed (or
// Locked) — Open is never written to disk.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EDoorState : uint8
{
    Locked   UMETA(DisplayName = "Locked"),    // Blocks pawn, cannot be opened without RequiredKey.
    Closed   UMETA(DisplayName = "Closed"),    // Blocks pawn, opens on next interact.
    Open     UMETA(DisplayName = "Open"),      // Walkable, will auto-close after AutoCloseDelaySeconds.
};

// ---------------------------------------------------------------------------
// ADoor
//
// Placed actor that represents a lockable door in the exploration mode.
// Implements:
//   - IInteractable    — opens/unlocks on interact line trace from the player.
//   - ISaveParticipant — persists the bIsUnlocked flag via SaveableComponent.
//
// State machine (auto-close model — option C):
//
//                +-------+ has key  +--------+  interact  +------+
//                |Locked | -------> | Closed | ---------> | Open |
//                +-------+   (key   +--------+            +------+
//                  ^   ^   consumed)    ^                     |
//                  |   |                |     timer (2.5s)    |
//                  |   +-- no key ------+---------------------+
//                  |       (locked
//                  |        feedback)
//                  +-- (Open never re-locks; bIsUnlocked is one-way.)
//
// Visuals are decoupled — C++ flips collision and fires BP_OnDoorOpening /
// BP_OnDoorClosing so the BP subclass can drive the actual animation
// (skeletal anim, timeline rotation, particles, whatever the LD wires up).
//
// Component layout:
//   Root (USceneComponent)
//   ├── DoorMesh          (USkeletalMeshComponent — VISUAL ONLY, NoCollision)
//   ├── DoorCollisionBox  (UBoxComponent          — solid blocker, sized in BP)
//   └── InteractionSphere (USphereComponent       — QueryOnly trace volume)
//
// IMPORTANT: a SkeletalMeshComponent has no collision bodies without a Physics
// Asset assigned. Even with response Block on every channel, a pawn passes
// straight through. The UBoxComponent is the actual collider — the mesh is
// purely cosmetic. This is the standard UE pattern for doors and simple props.
//
// The SceneComponent root keeps the actor pivot independent from the mesh
// pivot — a future swing animation can rotate DoorMesh around its hinge socket
// without dragging the box or the sphere along (they stay in place; collision
// is disabled while Open anyway).
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
    // Read access
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintPure, Category = "Door")
    EDoorState GetState() const { return CurrentState; }

    UFUNCTION(BlueprintPure, Category = "Door")
    bool IsLocked() const { return CurrentState == EDoorState::Locked; }

    UFUNCTION(BlueprintPure, Category = "Door")
    bool IsUnlocked() const { return bIsUnlocked; }

protected:

    virtual void BeginPlay() override;

    // -----------------------------------------------------------------------
    // Configuration — designer-tweakable per placement
    // -----------------------------------------------------------------------

    // If true, the door starts in the Locked state on a fresh save.
    // If false, it starts in Closed (already unlocked).
    // Once unlocked, this field is irrelevant — bIsUnlocked drives the post-load state.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Config")
    bool bStartLocked = true;

    // Item required to unlock this door. Consumed (one-time use) on success.
    // If null AND bStartLocked is true, a warning is logged at BeginPlay and
    // the door cannot be unlocked by interact (only by BP override / scripts).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Config")
    TSoftObjectPtr<UItemDefinition> RequiredKey;

    // Message shown to the player when interacting with a Locked door without
    // the right key. Falls back to a generic "It's locked." if left empty.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Config")
    FText LockedPromptText;

    // -----------------------------------------------------------------------
    // SFX overrides — null entries fall back to UAudioSubsystem defaults
    // -----------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|SFX")
    TObjectPtr<USoundBase> LockedAttemptSFX;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|SFX")
    TObjectPtr<USoundBase> UnlockSFX;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|SFX")
    TObjectPtr<USoundBase> OpenSFX;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|SFX")
    TObjectPtr<USoundBase> CloseSFX;

    // -----------------------------------------------------------------------
    // Components
    // -----------------------------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> Root;

    // Visual-only door leaf. NEVER configured for collision — see class comment.
    // BP subclass assigns the SkeletalMesh asset.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USkeletalMeshComponent> DoorMesh;

    // Solid collider that does the actual pawn blocking. Sized in the BP
    // subclass to match the visual door leaf (designer tunes BoxExtent and
    // RelativeLocation per door asset). Toggled on/off by ApplyCollisionForState.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> DoorCollisionBox;

    // Wider trigger volume for the interaction line trace. QueryOnly — never
    // physically obstructs the player, only returns hits to the trace.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;

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

    // -----------------------------------------------------------------------
    // Constants
    // -----------------------------------------------------------------------

    static constexpr float AutoCloseDelaySeconds = 2.5f;

    static const FName SaveKey;

    // -----------------------------------------------------------------------
    // Internal — state transitions
    // -----------------------------------------------------------------------

    void HandleUnlockAndOpen(AActor* Interactor);
    void HandleLockedAttempt();
    void TransitionToOpen();
    void TransitionToClosed();

    // Sets DoorCollisionBox + InteractionSphere collision modes for the given state.
    // - Locked / Closed: both enabled (box blocks pawn, sphere accepts trace).
    // - Open: both disabled (player walks through; trace ignores the door).
    //
    // DoorMesh is never touched here — it stays NoCollision for its entire lifetime.
    //
    // NOTE: Auto-close edge case — if a pawn is overlapping the door footprint
    // when the timer fires, the close transition will re-enable collision and
    // can clip the pawn. Acceptable for v1 (turn-based pace). When this becomes
    // a problem, gate TransitionToClosed on a UWorld::OverlapMultiByChannel
    // check and re-arm the timer if blocked.
    void ApplyCollisionForState(EDoorState State);

    void PlayDoorSFX(USoundBase* Override, USoundBase* Fallback, const TCHAR* DebugLabel) const;

    UAudioSubsystem* GetAudioSubsystem() const;

    // -----------------------------------------------------------------------
    // Timer
    // -----------------------------------------------------------------------

    FTimerHandle AutoCloseTimerHandle;
};