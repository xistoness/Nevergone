// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattlefieldQuerySubsystem.generated.h"

class AActor;

UCLASS()
class NEVERGONE_API UBattlefieldQuerySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	/** Called before combat preparation */
	void BuildCache();

	/** Clears all cached data */
	void InvalidateCache();

	/** --- Queries --- */

	void GetPlayerSpawnSlots(TArray<FTransform>& OutSlots) const;
	void GetEnemySpawnSlots(TArray<FTransform>& OutSlots) const;
	void GetCoverProviders(TArray<TWeakObjectPtr<AActor>>& OutCovers) const;

	bool HasCoverAtLocation(const FVector& Location) const;

	bool HasLineOfEffect(
	const FVector& From,
	const FVector& To,
	const TArray<AActor*>& IgnoredActors
) const;

	void QueryActorsInRadius(
		const FVector& Origin,
		float Radius,
		TArray<AActor*>& OutActors
	) const;

	void QueryActorsInCone(
		const FVector& Origin,
		const FVector& Forward,
		float AngleDegrees,
		float Distance,
		TArray<AActor*>& OutActors
	) const;

	
private:
	/** Cached data (derived from world scan) */
	UPROPERTY()
	TArray<FTransform> CachedPlayerSpawnSlots;

	UPROPERTY()
	TArray<FTransform> CachedEnemySpawnSlots;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> CachedCoverProviders;

	bool bCacheValid = false;
};
