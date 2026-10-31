// Copyright Xyzto Works

#include "World/ExcursionScentSubsystem.h"

#include "Actors/ExcursionScentBait.h"

#include "Engine/World.h"

UExcursionScentSubsystem* UExcursionScentSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;

	return World ? World->GetSubsystem<UExcursionScentSubsystem>() : nullptr;
}

bool UExcursionScentSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UExcursionScentSubsystem::RegisterScentBait(AExcursionScentBait* Bait)
{
	if (Bait)
	{
		ScentBaits.AddUnique(Bait);
	}
}

void UExcursionScentSubsystem::UnregisterScentBait(AExcursionScentBait* Bait)
{
	ScentBaits.Remove(Bait);
}

void UExcursionScentSubsystem::GatherScentBaitsAt(const FVector& Location, TArray<AExcursionScentBait*>& OutBaits) const
{
	OutBaits.Reset();

	for (const TWeakObjectPtr<AExcursionScentBait>& Weak : ScentBaits)
	{
		AExcursionScentBait* Bait = Weak.Get();

		if (IsValid(Bait) &&
			FVector::DistSquared(Bait->GetActorLocation(), Location) <= FMath::Square(Bait->GetScentRadius()))
		{
			OutBaits.Add(Bait);
		}
	}

	OutBaits.Sort([&Location](const AExcursionScentBait& A, const AExcursionScentBait& B)
	{
		return FVector::DistSquared(A.GetActorLocation(), Location) < FVector::DistSquared(B.GetActorLocation(), Location);
	});
}
