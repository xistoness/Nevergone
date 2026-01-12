// Copyright Xyzto Works


#include "GameMode/BattlefieldQuerySubsystem.h"

#include "EngineUtils.h"

void UBattlefieldQuerySubsystem::BuildCache()
{
	CachedPlayerSpawnSlots.Reset();
	CachedEnemySpawnSlots.Reset();
	CachedCoverProviders.Reset();
	
	GetPlayerSpawnSlots(CachedPlayerSpawnSlots);
	GetEnemySpawnSlots(CachedEnemySpawnSlots);
	GetCoverProviders(CachedCoverProviders);
	
	bCacheValid = true;
}

void UBattlefieldQuerySubsystem::InvalidateCache()
{
}

void UBattlefieldQuerySubsystem::GetPlayerSpawnSlots(TArray<FTransform>& OutSlots) const
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;

		//if (Actor->FindComponentByClass<UBattlefieldSpawnSlotComponent>())
		{
			// Read transform & classify
			FTransform TempTransform = Actor->GetTransform();
			OutSlots.Add(TempTransform);
		}
	}
	
}

void UBattlefieldQuerySubsystem::GetEnemySpawnSlots(TArray<FTransform>& OutSlots) const
{
}

void UBattlefieldQuerySubsystem::GetCoverProviders(TArray<TWeakObjectPtr<AActor>>& OutCovers) const
{
}

bool UBattlefieldQuerySubsystem::HasCoverAtLocation(const FVector& Location) const
{
	return false;
}


