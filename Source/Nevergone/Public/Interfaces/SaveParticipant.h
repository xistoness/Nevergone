// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SaveParticipant.generated.h"

struct FActorSaveData;

UINTERFACE(MinimalAPI)
class USaveParticipant : public UInterface
{
	GENERATED_BODY()
};

class NEVERGONE_API ISaveParticipant
{
	GENERATED_BODY()

public:
	
	// Allows the actor to write custom save data
	UFUNCTION(BlueprintNativeEvent, Category="Save")
	void WriteSaveData(FActorSaveData& OutData) const;

	// Allows the actor to restore custom save data
	UFUNCTION(BlueprintNativeEvent, Category="Save")
	void ReadSaveData(const FActorSaveData& InData);

	// Allows the actor to restore custom save data
	UFUNCTION(BlueprintNativeEvent, Category="Save")
	void OnPostRestore();
};
