// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/SaveParticipant.h"
#include "Types/BattleTypes.h"
#include "UnitStatsComponent.generated.h"

class UUnitDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnUnitDeath, ACharacterBase*);

// The 8 base attributes distributed by the player on level-up.
// These are persisted and drive concrete stat calculations.
// Equipment bonuses and future modifiers will be separate fields.
USTRUCT(BlueprintType)
struct FUnitAttributes
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, EditDefaultsOnly, BlueprintReadOnly)
    int32 Constitution = 0;

    UPROPERTY(SaveGame, EditDefaultsOnly, BlueprintReadOnly)
    int32 Strength = 0;

    UPROPERTY(SaveGame, EditDefaultsOnly, BlueprintReadOnly)
    int32 Dexterity = 0;

    UPROPERTY(SaveGame, EditDefaultsOnly, BlueprintReadOnly)
    int32 Knowledge = 0;

    UPROPERTY(SaveGame, EditDefaultsOnly, BlueprintReadOnly)
    int32 Focus = 0;

    UPROPERTY(SaveGame, EditDefaultsOnly, BlueprintReadOnly)
    int32 Technique = 0;

    UPROPERTY(SaveGame, EditDefaultsOnly, BlueprintReadOnly)
    int32 Evasiveness = 0;

    UPROPERTY(SaveGame, EditDefaultsOnly, BlueprintReadOnly)
    int32 Speed = 0;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
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

    // HP is persisted because it carries between exploration and battle.
    // Other derived stats are recomputed each battle from attributes + definition.
    UPROPERTY(SaveGame)
    float PersistentHP = 0.f;

    // The 8 base attributes invested by the player over level-ups.
    UPROPERTY(SaveGame)
    FUnitAttributes Attributes;

    // ===== Battle Session State =====
    // These are reset/assigned by CombatManager at battle start.
    // They are NOT persisted — BattleUnitState is the authority during combat.

    UPROPERTY(SaveGame)
    int32 AllyTeam = 0;

    UPROPERTY(SaveGame)
    int32 EnemyTeam = 1;

    UPROPERTY()
    int32 CurrentActionPoints = 0;

    // ===== Getters — persistent =====
    int32 GetAllyTeam() const;
    int32 GetEnemyTeam() const;
    float GetCurrentHP() const;
    bool IsAlive() const;
    const UUnitDefinition* GetDefinition() const { return Definition; }
    FGridTraversalParams GetTraversalParams() const;

    // ===== Getters — concrete stats derived from attributes + definition =====
    // These are used to POPULATE BattleUnitState at battle start.
    // During combat, read from BattleUnitState instead.
    float GetMaxHP() const;
    float GetPhysicalAttack() const;
    float GetRangedAttack() const;
    float GetMagicalPower() const;
    float GetPhysicalDefense() const;
    float GetMagicalDefense() const;
    int32 GetActionPoints() const;
    int32 GetMovementRange() const;
    int32 GetHitChanceModifier() const;
    int32 GetEvasionModifier() const;
    int32 GetCritChance() const;         // Returns percentage (0-100)

    // ===== Setters =====
    void SetAllyTeam(int32 Team);
    void SetEnemyTeam(int32 Team);

    // Raw HP write — clamps to [0, MaxHP] and fires OnUnitDeath if HP reaches zero.
    // All combat-facing HP changes must go through UCombatEventBus.
    void SetCurrentHP(float NewHP);
    void SetCurrentActionPoints(int32 ActionPoints);

    FOnUnitDeath OnUnitDeath;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};