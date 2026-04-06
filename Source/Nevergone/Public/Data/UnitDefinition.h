// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/BattleTypes.h"
#include "Types/CharacterTypes.h"
#include "Engine/DataAsset.h"
#include "UnitDefinition.generated.h"

UCLASS()
class NEVERGONE_API UUnitDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // --- Identity ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 MaxLevel = 30;

    // --- HP growth ---
    // Flat HP gained per level regardless of attributes.
    // MaxHP = (HPPerLevel * Level) + (Constitution * 5)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth", meta = (ClampMin = "0"))
    float HPPerLevel = 10.f;

    // --- Concrete stat growth per level ---
    // These represent the unit's class identity (a warrior grows PhysAtk faster than MagPwr).
    // Player attribute investment is added on top via the attribute formulas.

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth", meta = (ClampMin = "0"))
    float PhysAttkPerLevel = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth", meta = (ClampMin = "0"))
    float RangedAttkPerLevel = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth", meta = (ClampMin = "0"))
    float MagPwrPerLevel = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth", meta = (ClampMin = "0"))
    float PhysDefPerLevel = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Growth", meta = (ClampMin = "0"))
    float MagDefPerLevel = 0.f;

    // --- Flat base values (not level-scaled) ---

    // Base action points before Technique contribution.
    // ActionPoints = BaseActionPoints + (Technique / 5)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base", meta = (ClampMin = "1"))
    int32 BaseActionPoints = 2;

    // Base movement range before Speed contribution.
    // MovementRange = BaseMoveRange + Speed
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base", meta = (ClampMin = "1"))
    int32 BaseMoveRange = 3;

    // --- Grid traversal ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FGridTraversalParams TraversalParams;

    // --- Abilities ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle")
    TArray<FUnitAbilityEntry> BattleAbilities;

    // --- Immunities ---
    // Tags granted to the unit's ASC at battle start.
    // Convention: "Immunity.<StatusTagName_with_dots_as_underscores>"
    // Example: to make a unit immune to Stun (tag "State.Stunned"), add "Immunity.State_Stunned".
    // StatusEffectManager checks the ASC for these before applying any status effect.
    // Equipment, terrain, and other runtime effects can grant additional immunity tags at any time.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle")
    FGameplayTagContainer InnateImmunityTags;
};