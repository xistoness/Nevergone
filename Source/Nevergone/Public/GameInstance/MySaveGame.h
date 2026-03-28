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
	
	UPROPERTY()
	TMap<FName, FLevelSaveData> SavedActorsByLevel;
	
	// Display name shown in the Load Game list (e.g. "Floor 3 — Encounter 2")
	UPROPERTY(SaveGame)
	FString SaveDisplayName;
 
	// Total playtime in seconds — increment this in your game loop
	UPROPERTY(SaveGame)
	float PlaytimeSeconds = 0.f;
 
	// UTC timestamp of when this save was last written
	UPROPERTY(SaveGame)
	FDateTime LastSavedAt;
 
	// Which level the player was on when this was saved
	UPROPERTY(SaveGame)
	FName SavedLevelName = NAME_None;	
};
