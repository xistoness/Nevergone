// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Data/ActorSaveData.h"
#include "GameFramework/SaveGame.h"
#include "Data/PartyData.h"
#include "Data/ProgressionData.h"
#include "Types/BattleTypes.h"
#include "MySaveGame.generated.h"

// ---------------------------------------------------------------------------
// Mid-combat serialization structs
// ---------------------------------------------------------------------------

// Serialized form of a single active status effect instance.
// Stored as raw values so the effect can be restored without re-applying
// passive effects (which would double the stat change).
USTRUCT()
struct FSavedActiveStatusEffect
{
    GENERATED_BODY()

    // Soft path to the UStatusEffectDefinition data asset.
    UPROPERTY(SaveGame)
    FSoftObjectPath DefinitionPath;

    // Guid of the caster actor. May be invalid if the caster died before the save.
    UPROPERTY(SaveGame)
    FGuid CasterGuid;

    UPROPERTY(SaveGame)
    EBattleUnitTeam CasterTeam = EBattleUnitTeam::Ally;

    UPROPERTY(SaveGame)
    int32 TurnsRemaining = 1;

    UPROPERTY(SaveGame)
    int32 ShieldHP = 0;

    UPROPERTY(SaveGame)
    int32 CachedCasterStatValue = 0;

    UPROPERTY(SaveGame)
    int32 CachedMovementRangeSnapshot = 0;

    // Exact delta applied to a stat by IncreaseStat/DecreaseStat — needed for exact revert.
    UPROPERTY(SaveGame)
    int32 CachedStatDelta = 0;

    // Unique instance ID within the unit's active effect list.
    UPROPERTY(SaveGame)
    int32 InstanceId = INDEX_NONE;
};

// Serialized cooldown entry for one AbilityDefinition asset.
USTRUCT()
struct FSavedCooldown
{
    GENERATED_BODY()

    // Soft path to the UAbilityDefinition data asset.
    UPROPERTY(SaveGame)
    FSoftObjectPath DefinitionPath;

    // Stored turns remaining (same Model-A semantics as the live map).
    UPROPERTY(SaveGame)
    int32 TurnsRemaining = 0;
};

// Full serialized combat state for one unit.
USTRUCT()
struct FSavedCombatUnitState
{
    GENERATED_BODY()

    // Links this record to the actor in the world via its SaveableComponent guid.
    UPROPERTY(SaveGame)
    FGuid ActorGuid;

    UPROPERTY(SaveGame)
    int32 CurrentHP = 0;

    UPROPERTY(SaveGame)
    int32 CurrentActionPoints = 0;

    // Temporary attribute bonuses active at save time.
    // Already reflected in the stat values above — restored here so
    // RecalculateBattleStats() produces the same derived stats.
    UPROPERTY(SaveGame)
    FUnitAttributes TemporaryAttributeBonuses;

    UPROPERTY(SaveGame)
    TArray<FSavedActiveStatusEffect> ActiveStatusEffects;

    UPROPERTY(SaveGame)
    TArray<FSavedCooldown> Cooldowns;
};

// Top-level container written to MySaveGame when saving mid-combat.
USTRUCT()
struct FSavedCombatSession
{
    GENERATED_BODY()

    // Guid of the AFloorEncounterVolume that started this combat.
    // Used to re-locate the volume actor on restore so the grid can be rebuilt.
    UPROPERTY(SaveGame)
    FGuid EncounterVolumeGuid;

    UPROPERTY(SaveGame)
    TArray<FSavedCombatUnitState> UnitStates;

    void Reset()
    {
        EncounterVolumeGuid.Invalidate();
        UnitStates.Reset();
    }
};

// ---------------------------------------------------------------------------
// Save game object
// ---------------------------------------------------------------------------

UCLASS()
class NEVERGONE_API UMySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	
	UPROPERTY(SaveGame)
	FString SaveSlotName;
	
	UPROPERTY(SaveGame)
	FPartyData PartyData;

	UPROPERTY(SaveGame)
	FProgressionData ProgressionData;
	
	UPROPERTY(SaveGame)
	TMap<FName, bool> GlobalFlags;
	
	UPROPERTY(SaveGame)
	TMap<FName, FLevelSaveData> SavedActorsByLevel;
	
	// Display name shown in the Load Game list (e.g. "Floor 3 — Encounter 2")
	UPROPERTY(SaveGame)
	FString SaveDisplayName;
 
	// Total playtime in seconds — increment this in your game loop
	UPROPERTY(SaveGame)
	float PlaytimeSeconds = 0.f;
 
	// UTC timestamp of when this save was last written
	UPROPERTY(SaveGame)
	FDateTime LastSavedAt;
 
	// Which level the player was on when this was saved
	UPROPERTY(SaveGame)
	FName SavedLevelName = NAME_None;

    // ---------------------------------------------------------------------------
    // Mid-combat save state
    // ---------------------------------------------------------------------------

    // True when this save was created during an active combat session.
    // On load, GameContextManager uses this to skip BattlePreparation and
    // restore the combat directly from SavedCombatSession.
    UPROPERTY(SaveGame)
    bool bSavedMidCombat = false;

    UPROPERTY(SaveGame)
    FSavedCombatSession SavedCombatSession;
};