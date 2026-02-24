// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FloorEncounterVolume.generated.h"

class UFloorEncounterData;
class UBoxComponent;
class ABattleVolume;

UCLASS()
class NEVERGONE_API AFloorEncounterVolume : public AActor
{
	GENERATED_BODY()
	
public:	

	AFloorEncounterVolume();
	
	UPROPERTY(EditAnywhere, Category = "Encounter")
	TObjectPtr<UFloorEncounterData> EncounterData;

	UPROPERTY(EditAnywhere, Category = "Encounter")
	bool bAutoStartOnEnter = true;
	
	UPROPERTY(EditAnywhere, Category="Battle")
	ABattleVolume* BattleVolume;

	ABattleVolume* GetBattleVolume() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter")
	TObjectPtr<UBoxComponent> TriggerBox;

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
