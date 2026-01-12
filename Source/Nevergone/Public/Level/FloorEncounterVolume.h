// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FloorEncounterVolume.generated.h"

UCLASS()
class NEVERGONE_API AFloorEncounterVolume : public AActor
{
	GENERATED_BODY()
	
public:	

	AFloorEncounterVolume();

public:
	UPROPERTY(EditAnywhere)
	TObjectPtr<class UFloorEncounterData> EncounterData;

	UPROPERTY(EditAnywhere)
	bool bAutoStartOnEnter = true;

};
