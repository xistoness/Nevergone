// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/ActorSaveData.h"
#include "Saveable.generated.h"

// This class does not need to be modified.
UINTERFACE(Blueprintable)
class USaveable : public UInterface
{
	GENERATED_BODY()
};

class NEVERGONE_API ISaveable
{
	GENERATED_BODY()

public:
	// Returns the persistent GUID of this actor or creates one if there's none
	virtual FGuid GetOrCreateGuid() = 0;

	// Assigns a persistent GUID (used on spawn / restore)
	virtual void SetActorGuid(const FGuid& NewGuid) = 0;

	// Called before saving; actor fills SaveData
	UFUNCTION(BlueprintNativeEvent)
	void WriteSaveData(FActorSaveData& OutData) const;

	// Called after loading; actor restores internal state
	virtual void ReadSaveData(const FActorSaveData& InData) = 0;

	virtual TSoftClassPtr<AActor> GetActorClass() const = 0;
	
	// Optional hook after full world restoration
	virtual void OnPostRestore() {}
};
