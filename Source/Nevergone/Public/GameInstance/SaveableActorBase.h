// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Saveable.h"
#include "SaveableActorBase.generated.h"

UCLASS()
class NEVERGONE_API ASaveableActorBase : public AActor, public ISaveable
{
	GENERATED_BODY()

public:
	ASaveableActorBase();
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual FGuid GetOrCreateGuid() override;
	virtual void SetActorGuid(const FGuid& NewGuid) override;

	virtual void WriteSaveData_Implementation(FActorSaveData& OutData) const override;
	virtual void ReadSaveData(const FActorSaveData& InData) override;
	virtual TSoftClassPtr<AActor> GetActorClass() const override;


protected:
	// Persistent runtime GUID (mirrors SaveData)
	UPROPERTY(VisibleAnywhere, Category="Save")
	FGuid SaveGuid;
};
