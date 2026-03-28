// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/SaveParticipant.h"
#include "Types/BattleTypes.h"
#include "UnitStatsComponent.generated.h"

class UUnitDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnUnitDeath, ACharacterBase*);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class NEVERGONE_API UUnitStatsComponent : public UActorComponent, public ISaveParticipant
{
	GENERATED_BODY()

public:	
	
	UUnitStatsComponent();
	void InitializeForBattle();
	
	// ===== Save =====
	virtual void WriteSaveData_Implementation(FActorSaveData& OutData) const override;
	virtual void ReadSaveData_Implementation(const FActorSaveData& InData) override;

	// ===== Definition =====
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UUnitDefinition* Definition;

	// ===== Persistent State =====
	UPROPERTY(SaveGame)
	int32 Level = 1;

	UPROPERTY(SaveGame)
	float PersistentHP = 0.f;
	
	// ===== Temporary State =====
	
	UPROPERTY(SaveGame)
	int32 AllyTeam = 0;
	
	UPROPERTY(SaveGame)
	int32 EnemyTeam = 1;
	
	UPROPERTY(SaveGame)
	int32 CurrentActionPoints = 0;

	// ===== Derived Getters =====
	float GetMaxHP() const;
	float GetSpeed() const;
	float GetMeleeAttack() const;
	float GetRangedAttack() const;
	float GetActionPoints() const;
	float GetCurrentActionPoints() const;
	int32 GetAllyTeam() const;
	int32 GetEnemyTeam() const;
	float GetCurrentHP() const;
	FGridTraversalParams GetTraversalParams() const;
	const UUnitDefinition* GetDefinition() const { return Definition; }
	
	// ===== Derived Setters =====
	void SetAllyTeam(int32 Team);
	void SetEnemyTeam(int32 Team);

	/**
	 * Raw HP write — clamps to [0, MaxHP] and fires OnUnitDeath if HP
	 * reaches zero. Does NOT spawn floating text or notify BattleState.
	 * All combat-facing HP changes must go through UCombatEventBus.
	 */
	void SetCurrentHP(float NewHP);
	void SetCurrentActionPoints(int32 ActionPoints);

	bool IsAlive() const;
	
	FOnUnitDeath OnUnitDeath;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};