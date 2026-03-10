// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelPortal.generated.h"

class UBoxComponent;
class USceneComponent;
class UArrowComponent;

UCLASS()
class NEVERGONE_API ALevelPortal : public AActor
{
	GENERATED_BODY()

public:
	ALevelPortal();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal")
	FName PortalID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal")
	FName TargetLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal")
	FName TargetPortalID;

	// Returns the final entry transform used to place the player when entering this level
	UFUNCTION(BlueprintCallable, Category="Portal|Spawn")
	FTransform GetEntryTransform() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portal")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portal")
	UBoxComponent* Trigger;

	// Visual spawn marker (move/rotate this in editor)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portal|Spawn")
	USceneComponent* SpawnPoint;

	// Optional: arrow to visualize facing direction
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Portal|Spawn")
	UArrowComponent* SpawnArrow;

	UFUNCTION()
	void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);
};