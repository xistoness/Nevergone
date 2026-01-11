// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SaveableComponent.generated.h"


struct FActorSaveData;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class NEVERGONE_API USaveableComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	USaveableComponent();
	
	void OnRegister() override;
	
	// Returns the persistent GUID of this actor or creates one if there's none
	FGuid GetOrCreateGuid();

	// Assigns a persistent GUID (used on spawn / restore)
	void SetActorGuid(const FGuid& NewGuid);

	// Called before saving; actor fills SaveData
	void WriteSaveData(FActorSaveData& OutData) const;

	// Called after loading; actor restores internal state
	void ReadSaveData(const FActorSaveData& InData);

	TSoftClassPtr<AActor> GetActorClass() const;
	
	// Optional hook after full world restoration
	void OnPostRestore();

protected:
	
	// Persistent runtime GUID (mirrors SaveData)
	UPROPERTY(VisibleAnywhere, Category="Save")
	FGuid SaveGuid;
};
