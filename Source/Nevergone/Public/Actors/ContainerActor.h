// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "Interfaces/SaveParticipant.h"
#include "Interfaces/ItemSource.h"
#include "Types/ItemTypes.h"
#include "ContainerActor.generated.h"

class USphereComponent;
class UContainerDefinition;
class UInteractableComponent;
class USaveableComponent;
class UItemDefinition;

// ---------------------------------------------------------------------------
// AContainerActor
//
// Placed actor representing a chest, barrel, drawer, or any other in-world
// item storage. Implements:
//
//   - IInteractable   — opens the transfer UI when the player presses Interact
//   - ISaveParticipant — persists Items + bHasBeenLooted via SaveableComponent
//   - IItemSource     — exposes its contents to UItemContainerSubsystem
//
// First-open loot:
//   bHasBeenLooted starts false. On the first Interact, the optional loot
//   table on Definition is rolled once and the rolled items are added to
//   Items, then the flag is set. Save/restore preserves both Items and the
//   flag so future opens read the persisted state.
//
// No weight limit — see UContainerDefinition comment.
// ---------------------------------------------------------------------------
UCLASS(Blueprintable)
class NEVERGONE_API AContainerActor : public AActor,
                                      public IInteractable,
                                      public ISaveParticipant,
                                      public IItemSource
{
    GENERATED_BODY()

public:

    AContainerActor();

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
    // IItemSource
    // -----------------------------------------------------------------------
    virtual TArray<FInventoryEntry> GetItemEntries() const override { return Items; }
    virtual int32 GetMaxSlots() const override;
    virtual int32 GetUsedSlots() const override { return Items.Num(); }
    virtual bool HasItem(const UItemDefinition* ItemDef, int32 Quantity) const override;
    virtual bool CanAcceptItem(const UItemDefinition* ItemDef, int32 Quantity) const override;
    virtual EInventoryResult ReceiveTransfer(UItemDefinition* ItemDef, int32 Quantity) override;
    virtual EInventoryResult ProvideTransfer(UItemDefinition* ItemDef, int32 Quantity) override;
    virtual FText GetSourceDisplayName() const override;

    // -----------------------------------------------------------------------
    // Public read access used by the UI
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintPure, Category = "Container")
    UContainerDefinition* GetDefinition() const { return Definition; }

    // Sum of (Quantity * WeightPerUnit) across all entries.
    // Display-only — containers do not enforce a weight cap.
    UFUNCTION(BlueprintPure, Category = "Container")
    float GetTotalWeight() const;

    // -----------------------------------------------------------------------
    // Delegate fired after every successful AddItem / RemoveItem.
    // The transfer widget subscribes to refresh both panels.
    // -----------------------------------------------------------------------
    DECLARE_MULTICAST_DELEGATE(FOnContainerContentsChanged);
    FOnContainerContentsChanged OnContentsChanged;

protected:

    virtual void BeginPlay() override;

    // Archetype reference. Required.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container")
    TObjectPtr<UContainerDefinition> Definition;
    
    // Collision sphere for interaction detection.
    // Profile: QueryAndPhysics | ObjectType: WorldDynamic
    // Responses: Ignore Visibility/Camera/GridTrace, Overlap all others, Block Interactable.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* InteractionSphere;

    // Visual mesh. No collision — interaction is handled by InteractionSphere.
    // BP subclass must assign a mesh asset here.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* ContainerMesh;

    // Components — created in the constructor for editor visibility.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container")
    TObjectPtr<UInteractableComponent> InteractableComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container")
    TObjectPtr<USaveableComponent> SaveableComponent;

    // Persistent contents. SaveGame so it round-trips through FActorSaveData
    // when serialized via FObjectAndNameAsStringProxyArchive in WriteSaveData.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Container")
    TArray<FInventoryEntry> Items;

    // True once the loot table has been rolled. Prevents re-rolling on every
    // load. Persisted with the actor.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Container")
    bool bHasBeenLooted = false;

private:

    // Called from Interact_Implementation on the first open. Idempotent.
    void EnsureInitialized();

    // Adds Quantity units of ItemDef to Items, merging into an existing stack
    // when one has room. Returns the post-mutation result code without
    // broadcasting OnContentsChanged — callers must broadcast themselves so
    // bulk operations can fire one event for many adds (e.g. EnsureInitialized).
    EInventoryResult AddInternal(UItemDefinition* ItemDef, int32 Quantity);
};
