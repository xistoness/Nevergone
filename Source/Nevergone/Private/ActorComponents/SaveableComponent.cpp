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
	if (!Owner)
	{
		return;
	}

	OutData.ActorGuid  = SaveGuid;
	OutData.ActorClass = Owner->GetClass();
	OutData.Transform  = Owner->GetActorTransform();
	OutData.LevelName  = Owner->GetWorld()->GetOutermost()->GetFName();

	// IMPORTANT:
	// Do NOT reset CustomDataMap here.
	// Each SaveParticipant owns its own payload key.

	// Actor-level save
	if (Owner->GetClass()->ImplementsInterface(USaveParticipant::StaticClass()))
	{
		ISaveParticipant::Execute_WriteSaveData(Owner, OutData);
	}

	// Component-level save
	for (UActorComponent* Component : Owner->GetComponents())
	{
		if (!Component)
		{
			continue;
		}

		if (Component->GetClass()->ImplementsInterface(USaveParticipant::StaticClass()))
		{
			ISaveParticipant::Execute_WriteSaveData(Component, OutData);
		}
	}
}

void USaveableComponent::ReadSaveData(const FActorSaveData& InData)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	SaveGuid = InData.ActorGuid;
	Owner->SetActorTransform(InData.Transform);

	// Actor-level restore
	if (Owner->GetClass()->ImplementsInterface(USaveParticipant::StaticClass()))
	{
		ISaveParticipant::Execute_ReadSaveData(Owner, InData);
	}

	// Component-level restore
	for (UActorComponent* Component : Owner->GetComponents())
	{
		if (!Component)
		{
			continue;
		}

		if (Component->GetClass()->ImplementsInterface(USaveParticipant::StaticClass()))
		{
			ISaveParticipant::Execute_ReadSaveData(Component, InData);
		}
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

	for (UActorComponent* Component : Owner->GetComponents())
	{
		if (!Component) continue;

		if (Component->GetClass()->ImplementsInterface(USaveParticipant::StaticClass()))
		{
			ISaveParticipant::Execute_OnPostRestore(Component);
		}
	}
}

