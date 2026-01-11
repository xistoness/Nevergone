// Copyright Xyzto Works


#include "GameInstance/SaveableActorBase.h"


ASaveableActorBase::ASaveableActorBase()
{
	// GUID is generated lazily
}

void ASaveableActorBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	GetOrCreateGuid();
}

FGuid ASaveableActorBase::GetOrCreateGuid()
{
	if (!SaveGuid.IsValid())
	{
		SaveGuid = FGuid::NewGuid();
	}
	return SaveGuid;
}

void ASaveableActorBase::SetActorGuid(const FGuid& NewGuid)
{
	SaveGuid = NewGuid;
}

void ASaveableActorBase::WriteSaveData_Implementation(FActorSaveData& OutData) const
{
	OutData.ActorGuid = SaveGuid;
	OutData.ActorClass = GetActorClass();
	OutData.Transform = GetActorTransform();
	OutData.LevelName = GetWorld()->GetOutermost()->GetFName();
}

void ASaveableActorBase::ReadSaveData(const FActorSaveData& InData)
{
	SaveGuid = InData.ActorGuid;
	SetActorTransform(InData.Transform);
}

TSoftClassPtr<AActor> ASaveableActorBase::GetActorClass() const
{
	return GetClass();
}
