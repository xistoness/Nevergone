// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExcursionThrowComponent.generated.h"

class ACharacter;
class UAnimMontage;
class UInstancedStaticMeshComponent;
class UItemDefinition;
class UStaticMesh;
class UStaticMeshComponent;

// One thing the ring offers: the item and how many of it the inventory holds.
struct FThrowableOption
{
	UItemDefinition* Item = nullptr;

	int32 Quantity = 0;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnToolRingOpened, const TArray<FThrowableOption>& /*Options*/, int32 /*Highlight*/);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnToolRingHighlightChanged, int32 /*Highlight*/);

DECLARE_MULTICAST_DELEGATE(FOnToolRingClosed);

// Item is null and Quantity zero when nothing can be thrown.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnThrowableSelectionChanged, UItemDefinition* /*Item*/, int32 /*Quantity*/);

// ---------------------------------------------------------------------------
// UExcursionThrowComponent
//
// Throwing distractions and scent baits during an excursion. Holding the
// input aims: the arc the object will follow is predicted every frame and
// drawn as a row of markers ending on a landing marker. Releasing throws the
// selected item, which leaves the inventory at that moment. It flies as an
// AExcursionThrowable; a distraction lands as the item's own pickup, so
// picking it up again gives it back, and a scent bait lands as an
// AExcursionScentBait, which cannot be picked up.
//
// What is thrown is picked on the tool ring: holding its input opens a ring
// of everything throwable in the inventory (items tagged Item.Distraction or
// Item.ScentBait with a pickup to fly as), the mouse moves a virtual
// pointer over it or the gamepad stick points at a slice directly, and
// releasing chooses the highlighted slice. The world
// keeps running and the survivor can still walk; the camera stops, and
// nothing can be aimed or thrown while the ring is open. The choice stays
// for the next throws. When the selected item runs out the selection passes
// to the first one still available, and with none there is nothing to throw.
// The ring and the HUD icon listen to the delegates below.
//
// Driven by UExplorationModeComponent (BeginAim / ReleaseAim / CancelAim and
// the ring calls), so input keeps reaching the survivor only through
// IExplorationInputReceiver. While aiming the survivor turns with the camera
// and can still move.
//
// The arc is predicted against the object types the thrown object collides
// with, ignoring pawns, so where the landing marker sits is where it lands.
// The marker components are created on the first aim, not on every
// ACharacterBase that carries this component.
// ---------------------------------------------------------------------------
UCLASS(ClassGroup = (Excursion), meta = (BlueprintSpawnableComponent))
class NEVERGONE_API UExcursionThrowComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UExcursionThrowComponent();

	// Starts aiming. Tells the player when there is nothing to throw.
	void BeginAim();

	// Throws along the arc being shown. Does nothing when not aiming.
	void ReleaseAim();

	// Stops aiming and closes the ring without throwing or choosing (a screen
	// opened, the mode ended).
	void CancelAim();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Excursion|Throw")
	bool IsAiming() const { return bAiming; }

	// --- Tool ring ---

	// Opens the ring on the throwables in the inventory. Tells the player
	// when there is nothing to throw. Stops an aim in progress.
	void OpenRing();

	// Closes the ring and selects the highlighted slice. Does nothing when
	// the ring is not open.
	void CloseRing();

	bool IsRingOpen() const { return bRingOpen; }

	// Mouse delta while the ring is open (screen convention, Y down): moves
	// the virtual pointer. Does nothing when the ring is closed.
	void PointRing(const FVector2D& Delta);

	// Gamepad stick position while the ring is open (Y up): highlights the
	// slice it points at. Does nothing when the ring is closed.
	void PointRingStick(const FVector2D& Stick);

	// Re-reads the inventory and moves the selection if it no longer holds.
	// Broadcasts OnSelectionChanged when the item or its quantity changed.
	void RefreshSelection();

	// The item the next throw uses, null when nothing can be thrown.
	UItemDefinition* GetSelectedThrowable() const;

	int32 GetSelectedQuantity() const { return SelectedQuantity; }

	// The player's survivor keeps the selection (and so the HUD icon) current
	// as the inventory changes. Other bodies carrying this component do not.
	void SetTrackInventory(bool bTrack);

	FOnToolRingOpened OnRingOpened;

	FOnToolRingHighlightChanged OnRingHighlightChanged;

	FOnToolRingClosed OnRingClosed;

	FOnThrowableSelectionChanged OnSelectionChanged;

protected:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- Throw ---

	UPROPERTY(EditDefaultsOnly, Category = "Throw", meta = (ClampMin = "0.0"))
	float LaunchSpeed = 1200.0f;

	// Added to the camera pitch, so a level camera still lobs the object
	// instead of throwing it straight at the floor ahead.
	UPROPERTY(EditDefaultsOnly, Category = "Throw", meta = (Units = "Degrees"))
	float AimPitchBiasDegrees = 10.0f;

	// Socket the object leaves from. Without it, FallbackThrowOffset is used.
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	FName ThrowSocketName = TEXT("hand_r");

	// Launch point relative to the actor when the socket does not exist.
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	FVector FallbackThrowOffset = FVector(30.0f, 0.0f, 50.0f);

	// Optional. Played on release; the object is spawned on the same frame.
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	TObjectPtr<UAnimMontage> ThrowMontage = nullptr;

	// --- Arc ---

	UPROPERTY(EditDefaultsOnly, Category = "Throw|Arc", meta = (ClampMin = "0.1", Units = "Seconds"))
	float MaxPredictionSeconds = 3.0f;

	// Drawn at every predicted point of the arc.
	UPROPERTY(EditDefaultsOnly, Category = "Throw|Arc")
	TObjectPtr<UStaticMesh> ArcMarkerMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Throw|Arc")
	FVector ArcMarkerScale = FVector(0.05f);

	// Drawn where the arc hits, lying on the surface.
	UPROPERTY(EditDefaultsOnly, Category = "Throw|Arc")
	TObjectPtr<UStaticMesh> LandingMarkerMesh = nullptr;

	// --- Ring ---

	// Pointer units per unit of mouse delta. Provisional.
	UPROPERTY(EditDefaultsOnly, Category = "Throw|Ring", meta = (ClampMin = "0.0"))
	float RingPointerScale = 10.0f;

	// The mouse pointer cannot go farther from the centre than this.
	UPROPERTY(EditDefaultsOnly, Category = "Throw|Ring", meta = (ClampMin = "1.0"))
	float RingPointerRadius = 100.0f;

	// Inside this distance of the mouse pointer from the centre the
	// highlight stays where it was.
	UPROPERTY(EditDefaultsOnly, Category = "Throw|Ring", meta = (ClampMin = "0.0"))
	float RingDeadZone = 30.0f;

	// Stick tilt (0 to 1) below which the highlight stays where it was, so
	// letting the stick back to the centre keeps the last slice. Provisional.
	UPROPERTY(EditDefaultsOnly, Category = "Throw|Ring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RingStickDeadZone = 0.5f;

private:

	// Every item in the inventory tagged Item.Distraction or Item.ScentBait
	// that has a pickup to fly as, in inventory order, stacks added up.
	void CollectThrowables(TArray<FThrowableOption>& OutOptions) const;

	// Moves the selection to Preferred when it is still available, else
	// keeps the current one, else the first available.
	void ResolveSelection(UItemDefinition* Preferred);

	void HandleInventoryChanged();

	// Moves the ring highlight to Slice and tells the ring. INDEX_NONE keeps it.
	void SetRingHighlight(int32 Slice);

	bool ComputeLaunch(FVector& OutStart, FVector& OutVelocity) const;

	void UpdateArc();

	void StopAiming();

	void EnsureArcComponents();

	void HideArc();

	// While aiming the survivor faces the camera instead of its movement.
	void SetAimRotation(bool bAim);

	void ShowPrompt(const FText& Message) const;

	ACharacter* GetCharacter() const;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ArcMarkers = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LandingMarker = nullptr;

	bool bAiming = false;

	// --- Ring state ---

	bool bRingOpen = false;

	TArray<FThrowableOption> RingOptions;

	// Keeps the items in RingOptions alive while the ring is open: the
	// options hold plain pointers to definitions loaded from soft pointers.
	UPROPERTY()
	TArray<TObjectPtr<UItemDefinition>> RingItemRefs;

	FVector2D RingPointer = FVector2D::ZeroVector;

	int32 RingHighlight = INDEX_NONE;

	TWeakObjectPtr<UItemDefinition> SelectedItem;

	int32 SelectedQuantity = 0;

	bool bTrackingInventory = false;

	FDelegateHandle InventoryChangedHandle;

	bool bSavedOrientRotationToMovement = true;

	bool bSavedUseControllerDesiredRotation = false;
};
