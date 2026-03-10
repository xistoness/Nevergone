// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Data/ActorSaveData.h"
#include "GameFramework/SaveGame.h"
#include "Data/PartyData.h"
#include "Data/ProgressionData.h"
#include "MySaveGame.generated.h"


UCLASS()
class NEVERGONE_API UMySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	
	UPROPERTY(SaveGame)
	FString SaveSlotName;
	
	UPROPERTY(SaveGame)
	FPartyData PartyData;

	UPROPERTY(SaveGame)
	FProgressionData ProgressionData;
	
	UPROPERTY(SaveGame)
	TMap<FName, bool> GlobalFlags;
	
	UPROPERTY(SaveGame)
	TArray<FActorSaveData> SavedActors;
	
};
