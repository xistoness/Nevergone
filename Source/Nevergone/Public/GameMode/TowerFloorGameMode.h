// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TowerFloorGameMode.generated.h"

class ACharacterBase;

UENUM(BlueprintType)
enum class EFloorCombatPolicy : uint8
{
	NoCombat,
	SingleCombat,
	MultipleCombats
};

enum class EGameContextState : uint8;

UCLASS()
class NEVERGONE_API ATowerFloorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATowerFloorGameMode();

	/** Structural rules of the floor */
	virtual bool AllowsExploration() const { return true; }
	virtual bool AllowsCombat() const { return CombatPolicy != EFloorCombatPolicy::NoCombat; }
	virtual bool AllowsMultipleCombats() const { return CombatPolicy == EFloorCombatPolicy::MultipleCombats; }

	/** Combat configuration */
	EFloorCombatPolicy GetCombatPolicy() const { return CombatPolicy; }

protected:
	/** Defines how combat behaves on this floor */
	UPROPERTY(EditDefaultsOnly, Category = "Floor Rules")
	EFloorCombatPolicy CombatPolicy = EFloorCombatPolicy::MultipleCombats;
	
	UPROPERTY(EditDefaultsOnly, Category = "Controllers")
	TSubclassOf<APlayerController> ExplorationControllerClass;

	UPROPERTY(EditDefaultsOnly, Category = "Controllers")
	TSubclassOf<APlayerController> BattleControllerClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Controllers")
	TSubclassOf<APlayerController> BattlePreparationControllerClass;
	
	UPROPERTY()
	APlayerController* ActivePlayerController;
	
	//Debug
	UPROPERTY(EditDefaultsOnly, Category="Debug|Party")
	TSubclassOf<ACharacterBase> TestCharClass;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	/** Called once when the level becomes active */
	virtual void SetupFloor();

	/** Called once before the level is unloaded */
	virtual void TeardownFloor();
	
	void HandleGameContextChanged(EGameContextState NewState);

	void SwitchPlayerController(TSubclassOf<APlayerController> NewControllerClass);
	
	void SwitchToExplorationController();
	void SwitchToBattlePreparationController();
	void SwitchToBattleController();

	APlayerController* SpawnAndSwapPlayerController(TSubclassOf<APlayerController> NewControllerClass);
	
};
