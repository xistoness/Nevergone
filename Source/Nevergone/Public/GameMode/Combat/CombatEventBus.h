// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "CombatEventBus.generated.h"

class ACharacterBase;
class UBattleState;

// ---------------------------------------------------------------------------
// Delegates — subscribe to these from anywhere inside the combat session
// ---------------------------------------------------------------------------

DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnDamageApplied,
	ACharacterBase* /*Source*/,
	ACharacterBase* /*Target*/,
	float           /*Amount*/
);

DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnHealApplied,
	ACharacterBase* /*Source*/,
	ACharacterBase* /*Target*/,
	float           /*Amount*/
);

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnStatusApplied,
	ACharacterBase*        /*Target*/,
	const FGameplayTag&    /*StatusTag*/
);

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnStatusCleared,
	ACharacterBase*        /*Target*/,
	const FGameplayTag&    /*StatusTag*/
);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnUnitDied,
	ACharacterBase* /*Unit*/
);

// ---------------------------------------------------------------------------

/**
 * Central event bus for in-combat notifications.
 *
 * Lives on UCombatManager for the duration of a battle session.
 * Abilities call Notify* methods; interested systems (BattleState,
 * FloatingText, animations, sound) subscribe via the public delegates.
 *
 * Adding a new event type only requires:
 *   1. A new delegate declaration above.
 *   2. A new Notify* method here.
 *   3. The caller (ability / status system) to invoke it.
 * No other existing code needs to change.
 */
UCLASS()
class NEVERGONE_API UCombatEventBus : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Wires the bus to the BattleState so HP mutations and death checks
	 * are handled automatically. Must be called once after both objects exist.
	 */
	void Initialize(UBattleState* InBattleState);

	// -----------------------------------------------------------------------
	// Notify methods — called by abilities and status systems
	// -----------------------------------------------------------------------

	/**
	 * Applies damage from Source to Target.
	 * Mutates UnitStatsComponent, syncs BattleState, triggers floating text,
	 * broadcasts FOnDamageApplied, and fires FOnUnitDied if HP reaches zero.
	 */
	void NotifyDamageApplied(ACharacterBase* Source, ACharacterBase* Target, float Amount);

	/**
	 * Applies healing from Source to Target.
	 * Mutates UnitStatsComponent, syncs BattleState, triggers floating text,
	 * and broadcasts FOnHealApplied.
	 */
	void NotifyHealApplied(ACharacterBase* Source, ACharacterBase* Target, float Amount);

	/**
	 * Applies a status tag to Target.
	 * Syncs BattleState and broadcasts FOnStatusApplied.
	 * Also triggers a floating status text on the target character.
	 *
	 * @param DisplayLabel  Short string shown as floating text (ex: "STUN")
	 * @param Icon          Optional icon texture for the floating widget
	 */
	void NotifyStatusApplied(
		ACharacterBase*     Target,
		const FGameplayTag& StatusTag,
		const FString&      DisplayLabel,
		UTexture2D*         Icon = nullptr
	);

	/**
	 * Clears a status tag from Target.
	 * Syncs BattleState and broadcasts FOnStatusCleared.
	 */
	void NotifyStatusCleared(ACharacterBase* Target, const FGameplayTag& StatusTag);

	// -----------------------------------------------------------------------
	// Delegates — subscribe from CombatManager, UI, audio, etc.
	// -----------------------------------------------------------------------

	FOnDamageApplied  OnDamageApplied;
	FOnHealApplied    OnHealApplied;
	FOnStatusApplied  OnStatusApplied;
	FOnStatusCleared  OnStatusCleared;
	FOnUnitDied       OnUnitDied;

private:

	UPROPERTY()
	TObjectPtr<UBattleState> BattleState;
};