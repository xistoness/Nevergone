// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "Interfaces/SaveParticipant.h"
#include "WorldItemActor.generated.h"

class UInteractableComponent;
class UItemDefinition;
class USaveableComponent;
class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;

// ---------------------------------------------------------------------------
// AWorldItemActor
//
// Represents any item that exists as a physical pickup actor in the world.
// Handles both level-placed collectibles and runtime-dropped items.
//
// Lifetime modes (controlled by VisitsRemaining):
//   VisitsRemaining = -1 (default): level-placed item, no expiry.
//     Collected → saved as absent → destroyed on next restore (discrepancy check).
//   VisitsRemaining >= 0: dropped item with visit-based lifetime.
//     Each save decrements the counter. When it reaches 0, bPersistAcrossLoads
//     is set to false and the actor is excluded from the next SpawnMissingActors.
//
// Blueprint subclass responsibilities:
//   - Assign PickupMesh (required for visibility).
//   - Implement BP_OnVisualsReady to update mesh and nameplate when ItemDefinition is set.
//   - Optionally implement BP_OnPickupFailed to show feedback (inventory full, etc.).
// ---------------------------------------------------------------------------
UCLASS(Blueprintable)
class NEVERGONE_API AWorldItemActor : public AActor, public IInteractable, public ISaveParticipant
{
    GENERATED_BODY()

public:

    AWorldItemActor();

    // Sets the item this actor represents and refreshes visuals.
    // Called automatically by InitializeAsDroppedItem; call manually for level-placed items.
    UFUNCTION(BlueprintCallable, Category = "Item")
    void SetItemDefinition(UItemDefinition* InItemDef);

    UFUNCTION(BlueprintPure, Category = "Item")
    UItemDefinition* GetItemDefinition() const;

    // The visual mesh. Read from a class default object, it is how anything
    // else can show this item exactly as its pickup looks (a thrown
    // distraction in flight).
    UStaticMeshComponent* GetPickupMesh() const { return PickupMesh; }

    // Initializes this actor as a runtime-dropped item.
    // Sets ItemDefinition, VisitsRemaining from ItemDef->DropVisitLifetime,
    // and bPersistAcrossLoads = true so the save system tracks it.
    // Called by InventorySubsystem::DropItem immediately after spawning.
    UFUNCTION(BlueprintCallable, Category = "Item")
    void InitializeAsDroppedItem(UItemDefinition* InItemDef);

    // IInteractable
    virtual void Interact_Implementation(AActor* Interactor) override;

    // ISaveParticipant
    virtual void WriteSaveData_Implementation(FActorSaveData& OutData) const override;
    virtual void ReadSaveData_Implementation(const FActorSaveData& InData) override;
    virtual void OnPostRestore_Implementation() override;

protected:

    virtual void BeginPlay() override;

    // -----------------------------------------------------------------------
    // Components
    // -----------------------------------------------------------------------

    // Collision sphere for interaction detection.
    // Profile: QueryAndPhysics | ObjectType: WorldDynamic
    // Responses: Ignore Visibility/Camera/GridTrace, Overlap all others, Block Interactable.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* InteractionSphere;

    // Visual mesh. No collision — interaction is handled by InteractionSphere.
    // BP subclass must assign a mesh asset here.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* PickupMesh;

    // Floating nameplate displayed above the actor.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UWidgetComponent* Nameplate;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UInteractableComponent* InteractableComponent;

    // Registers this actor with the save system.
    // bPersistAcrossLoads defaults to false for level-placed items (no respawn on load).
    // Set to true by InitializeAsDroppedItem so dropped items respawn until visits expire.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USaveableComponent* SaveableComponent;

    // -----------------------------------------------------------------------
    // Item data
    // -----------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    TSoftObjectPtr<UItemDefinition> ItemDefinition;

    // How many more level unloads this actor survives before being removed.
    // -1 = level-placed item (no expiry, removed when collected).
    // >= 0 = dropped item (decremented each save, removed when reaches 0).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
    int32 VisitsRemaining = -1;

    // -----------------------------------------------------------------------
    // Blueprint events
    // -----------------------------------------------------------------------

    // Called after ItemDefinition is set — BP updates mesh, nameplate text, etc.
    UFUNCTION(BlueprintImplementableEvent, Category = "Item")
    void BP_OnVisualsReady(UItemDefinition* ItemDef);

    // Called when PickUp fails (e.g. inventory full, overweight).
    UFUNCTION(BlueprintImplementableEvent, Category = "Item")
    void BP_OnPickupFailed(EInventoryResult Result);

    // -----------------------------------------------------------------------
    // Internal
    // -----------------------------------------------------------------------

    virtual void PickUp(AActor* Interactor);
    void RefreshVisuals();

private:

    static const FName SaveKey_ItemPath;
    static const FName SaveKey_VisitsRemaining;
};
