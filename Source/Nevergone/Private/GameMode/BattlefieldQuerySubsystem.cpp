// Copyright Xyzto Works


#include "GameMode/BattlefieldQuerySubsystem.h"

#include "EngineUtils.h"
#include "Actors/BattlefieldAllySpawnPoint.h"
#include "Actors/BattlefieldEnemySpawnPoint.h"

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
	for (TActorIterator<ABattlefieldAllySpawnPoint> It(GetWorld()); It; ++It)
	{
		FTransform TempTransform = It->GetTransform();
		OutSlots.Add(TempTransform);
	}	
}

void UBattlefieldQuerySubsystem::GetEnemySpawnSlots(TArray<FTransform>& OutSlots) const
{
	for (TActorIterator<ABattlefieldEnemySpawnPoint> It(GetWorld()); It; ++It)
	{
		FTransform TempTransform = It->GetTransform();
		OutSlots.Add(TempTransform);
	}
}

void UBattlefieldQuerySubsystem::GetCoverProviders(TArray<TWeakObjectPtr<AActor>>& OutCovers) const
{
}

bool UBattlefieldQuerySubsystem::HasCoverAtLocation(const FVector& Location) const
{
	return false;
}

bool UBattlefieldQuerySubsystem::HasLineOfEffect(const FVector& From, const FVector& To,
	const TArray<AActor*>& IgnoredActors) const
{
	return false;
}

void UBattlefieldQuerySubsystem::QueryActorsInRadius(const FVector& Origin, float Radius,
	TArray<AActor*>& OutActors) const
{
}

void UBattlefieldQuerySubsystem::QueryActorsInCone(const FVector& Origin, const FVector& Forward, float AngleDegrees,
	float Distance, TArray<AActor*>& OutActors) const
{
}


