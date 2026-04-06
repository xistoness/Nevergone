// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ActorComponents/SaveableComponent.h"
#include "Interfaces/SaveParticipant.h"
#include "FloorEncounterVolume.generated.h"

class UFloorEncounterData;
class UBoxComponent;
class UStaticMeshComponent;

/**
 * Self-contained encounter actor.
 *
 * TriggerBox  — detects the player and starts the battle sequence.
 * BattleBox   — defines the grid area (replaces the separate ABattleVolume actor).
 *
 * Both boxes are sized and positioned directly in the viewport on this
 * single actor — no external actor reference required.
 */
UCLASS()
class NEVERGONE_API AFloorEncounterVolume : public AActor, public ISaveParticipant
{
	GENERATED_BODY()
	
public:	

	AFloorEncounterVolume();
	
	// Save Game functions
	virtual void WriteSaveData_Implementation(FActorSaveData& OutData) const override;
	virtual void ReadSaveData_Implementation(const FActorSaveData& InData) override;
	virtual void OnPostRestore_Implementation() override;
	
	void DeactivateEncounter();

	// --- Grid config accessors (consumed by GridManager) ---

	FVector GetBattleBoxOrigin() const;
	FVector GetBattleBoxExtent() const;
	float   GetTileSize()        const { return TileSize; }
	float   GetGridHeight()      const { return GridHeight; }

	UPROPERTY(EditAnywhere, Category = "Encounter")
	TObjectPtr<UFloorEncounterData> EncounterData;

	UPROPERTY(EditAnywhere, Category = "Encounter")
	bool bAutoStartOnEnter = true;

	UPROPERTY()
	bool bEncounterResolved = false;

protected:

	/** Detects player entry and triggers the encounter. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter")
	TObjectPtr<UBoxComponent> TriggerBox;

	/**
	 * Defines the tactical grid area for this encounter.
	 * Resize this box in the viewport to control how many tiles are generated.
	 * The box must cover walkable ground — tile heights are detected via line trace.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle Grid")
	TObjectPtr<UBoxComponent> BattleBox;

	/** Size of each grid tile in Unreal units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle Grid", meta = (ClampMin = "50"))
	float TileSize = 100.f;

	/**
	 * Vertical search range for tile height detection (line trace half-height).
	 * Increase if your terrain has tall slopes or multi-level geometry.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle Grid", meta = (ClampMin = "100"))
	float GridHeight = 500.f;

	/** Visual indicator mesh. Animates with a gentle bob and slow Z-axis rotation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter")
	TObjectPtr<UStaticMeshComponent> IndicatorMesh;

	// -----------------------------------------------------------------------
	// Indicator animation parameters — tweak in the Blueprint CDO or viewport
	// -----------------------------------------------------------------------

	/** Total vertical travel distance of the bob (peak-to-peak), in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Animation", meta = (ClampMin = "0"))
	float BobAmplitude = 15.f;

	/** How many full bob cycles occur per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Animation", meta = (ClampMin = "0"))
	float BobFrequency = 0.6f;

	/** Degrees per second for the slow Z-axis rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Animation", meta = (ClampMin = "0"))
	float RotationSpeed = 30.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
	USaveableComponent* SaveableComponent;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:

	// Stores the mesh's base Z position (set in BeginPlay) so the bob is applied
	// as a delta on top of the intended placement rather than drifting over time.
	float MeshBaseZ = 0.f;

	// Accumulated time for the sine wave — wrapped at 2PI to avoid float
	// precision loss in very long sessions.
	float BobTime = 0.f;

protected:
	UFUNCTION()
	void OnTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);
};