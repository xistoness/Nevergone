// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "BattleVolume.generated.h"

UCLASS()
class NEVERGONE_API ABattleVolume : public AActor
{
	GENERATED_BODY()
	
public:
	ABattleVolume();

	UFUNCTION(BlueprintCallable, Category="Battle Grid")
	FVector GetOrigin() const;

	UFUNCTION(BlueprintCallable, Category="Battle Grid")
	FVector GetExtent() const;

	UFUNCTION(BlueprintCallable, Category="Battle Grid")
	float GetTileSize() const { return TileSize; }

	UFUNCTION(BlueprintCallable, Category="Battle Grid")
	float GetGridHeight() const { return GridHeight; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Collision")
	UBoxComponent* BoxComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle Grid", meta=(ClampMin="50"))
	float TileSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle Grid", meta=(ClampMin="100"))
	float GridHeight;
};
