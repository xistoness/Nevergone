// Copyright Xyzto Works


#include "ActorComponents/SaveableComponent.h"
#include "Data/ActorSaveData.h"
#include "Interfaces/SaveParticipant.h"
#include "GameFramework/Actor.h"

USaveableComponent::USaveableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USaveableComponent::OnRegister()
{
	Super::OnRegister();
	GetOrCreateGuid();
}

FGuid USaveableComponent::GetOrCreateGuid()
{
	if (!SaveGuid.IsValid())
	{
		SaveGuid = FGuid::NewGuid();
	}
	return SaveGuid;
}

void USaveableComponent::SetActorGuid(const FGuid& NewGuid)
{
	SaveGuid = NewGuid;
}

void USaveableComponent::WriteSaveData(FActorSaveData& OutData) const
{
	AActor* Owner = GetOwner();
	if (!Owner) return;
	
	OutData.ActorGuid  = SaveGuid;
	OutData.ActorClass = Owner->GetClass();
	OutData.Transform  = Owner->GetActorTransform();
	OutData.LevelName  = Owner->GetWorld()->GetOutermost()->GetFName();

	OutData.CustomData.Reset();

	if (Owner->GetClass()->ImplementsInterface(USaveParticipant::StaticClass()))
	{
		ISaveParticipant::Execute_WriteSaveData(Owner, OutData);
	}
}

void USaveableComponent::ReadSaveData(const FActorSaveData& InData)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	SaveGuid = InData.ActorGuid;
	Owner->SetActorTransform(InData.Transform);

	// Delegate restore
	if (Owner->GetClass()->ImplementsInterface(USaveParticipant::StaticClass()))
	{
		ISaveParticipant::Execute_ReadSaveData(Owner, InData);
	}
}

TSoftClassPtr<AActor> USaveableComponent::GetActorClass() const
{
	return GetClass();
}

void USaveableComponent::OnPostRestore()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (Owner->GetClass()->ImplementsInterface(USaveParticipant::StaticClass()))
	{
		ISaveParticipant::Execute_OnPostRestore(Owner);
	}
}

