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

	// No-op here: the save id is resolved lazily in GetSaveIdentifier().
	// This avoids generating a runtime id for placed actors that will use
	// their actor FName instead, and avoids relying on level-asset serialization
	// of any custom GUID property.
}

// ---------------------------------------------------------------------------
// Placed-actor detection
//
// A placed actor is one that exists in the level package — its FName is
// serialized in the level asset and is stable across sessions. We must
// detect this reliably to use the FName as the save identifier.
//
// Two flags can indicate "placed":
//   - RF_WasLoaded: set when the actor was deserialized from a package
//     via LoadPackage. Reliable in shipping builds and OpenLevel transitions.
//     NOT reliably set in PIE when the world is duplicated rather than loaded.
//   - bNetStartup (IsNetStartupActor): set when the actor was sorted into
//     the level's startup actor list. Reliable in PIE for placed actors,
//     but can be false for World Partition cell-streamed actors.
//
// Checking BOTH covers all cases: PIE, packaged builds, World Partition.
// If either flag is true, the actor is placed and uses its stable FName.
// ---------------------------------------------------------------------------
static bool IsPlacedActor(const AActor* Owner)
{
	if (!Owner) { return false; }
	return Owner->HasAnyFlags(RF_WasLoaded) || Owner->IsNetStartupActor();
}

FName USaveableComponent::GetSaveIdentifier()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return NAME_None;
	}

	if (IsPlacedActor(Owner))
	{
		return Owner->GetFName();
	}

	// RUNTIME-SPAWNED actors: generate (or reuse) a runtime save id.
	if (RuntimeSaveId.IsNone())
	{
		RuntimeSaveId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UE_LOG(LogTemp, Verbose,
			TEXT("[SaveableComponent] GetSaveIdentifier: generated runtime id '%s' for spawned actor '%s'."),
			*RuntimeSaveId.ToString(), *Owner->GetName());
	}
	return RuntimeSaveId;
}

void USaveableComponent::SetSaveIdentifier(FName NewIdentifier)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (IsPlacedActor(Owner))
	{
		// Placed actors are identified by GetFName() — never overwrite.
		UE_LOG(LogTemp, Warning,
			TEXT("[SaveableComponent] SetSaveIdentifier ignored for placed actor '%s' (uses FName)."),
			*Owner->GetName());
		return;
	}

	RuntimeSaveId = NewIdentifier;
}

void USaveableComponent::WriteSaveData(FActorSaveData& OutData) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Resolve the identifier — const_cast because GetSaveIdentifier is non-const
	// (it lazily initializes RuntimeSaveId), but the operation is idempotent.
	const FName Identifier = const_cast<USaveableComponent*>(this)->GetSaveIdentifier();

	OutData.ActorIdentifier    = Identifier;
	OutData.ActorClass         = Owner->GetClass();
	OutData.Transform          = Owner->GetActorTransform();
	OutData.LevelName          = Owner->GetWorld()->GetOutermost()->GetFName();
	OutData.bPersistAcrossLoads = bPersistAcrossLoads;

	UE_LOG(LogTemp, Log,
		TEXT("[SaveableComponent] WriteSaveData: actor='%s' id='%s' placed=%d bPersistAcrossLoads=%d"),
		*Owner->GetName(),
		*Identifier.ToString(),
		(int32)IsPlacedActor(Owner),
		(int32)bPersistAcrossLoads);

	// IMPORTANT:
	// Do NOT reset CustomDataMap here. Each SaveParticipant owns its own payload key.

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

	UE_LOG(LogTemp, Log,
		TEXT("[SaveableComponent] ReadSaveData: actor='%s' id='%s' placed=%d"),
		*Owner->GetName(),
		*InData.ActorIdentifier.ToString(),
		(int32)IsPlacedActor(Owner));

	// For spawned actors, reclaim the runtime save id from the save data.
	// For placed actors, this is a no-op (their id is GetFName()).
	if (!IsPlacedActor(Owner))
	{
		RuntimeSaveId = InData.ActorIdentifier;
	}

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