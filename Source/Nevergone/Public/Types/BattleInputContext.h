// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "BattleInputContext.generated.h"

/**
 * Who owns the current turn
 */
UENUM(BlueprintType)
enum class EBattleTurnOwner : uint8
{
	Player,
	Enemy,
	System
};

/**
 * High-level phase of the battle turn
 */
UENUM(BlueprintType)
enum class EBattleTurnPhase : uint8
{
	AwaitingOrders,     // Player can issue commands
	ExecutingActions,   // Animations, movement, abilities resolving
	Resolving,          // Damage, status effects, deaths
	Transition          // Switching turns
};

/**
 * What the player is currently interacting with
 */
UENUM(BlueprintType)
enum class EBattleInteractionMode : uint8
{
	None,
	SelectingUnit,
	ChoosingAction,
	Targeting,
	UIOnly
};

/**
 * Which system has priority for receiving input
 */
UENUM(BlueprintType)
enum class EBattleInputFocus : uint8
{
	None,
	UI,
	Unit,
};

/**
 * Immutable snapshot describing the current battle input context.
 * This struct is read by BattleInputManager to route player intentions.
 */
USTRUCT(BlueprintType)
struct FBattleInputContext
{
	GENERATED_BODY()

	/* ----- Turn state ----- */

	UPROPERTY(BlueprintReadOnly)
	EBattleTurnOwner TurnOwner = EBattleTurnOwner::Player;

	UPROPERTY(BlueprintReadOnly)
	EBattleTurnPhase TurnPhase = EBattleTurnPhase::AwaitingOrders;


	/* ----- Interaction state ----- */

	UPROPERTY(BlueprintReadOnly)
	EBattleInteractionMode InteractionMode = EBattleInteractionMode::None;


	/* ----- Input focus ----- */

	UPROPERTY(BlueprintReadOnly)
	EBattleInputFocus InputFocus = EBattleInputFocus::None;


	/* ----- Locks ----- */
		
	UPROPERTY(BlueprintReadOnly)
	bool bUnitInputEnabled = true;
	
	UPROPERTY()
	bool bCameraInputEnabled = true;


	/* ----- Helper queries ----- */

	bool IsPlayerTurn() const
	{
		return TurnOwner == EBattleTurnOwner::Player;
	}

	bool CanAcceptOrders() const
	{
		return IsPlayerTurn()
			&& TurnPhase == EBattleTurnPhase::AwaitingOrders
			&& bUnitInputEnabled;
	}
	
	bool CanAcceptCameraInput() const
	{
		return IsPlayerTurn()
			&& bCameraInputEnabled;
	}

	bool IsUIFocused() const
	{
		return InputFocus == EBattleInputFocus::UI;
	}

	bool IsUnitFocused() const
	{
		return InputFocus == EBattleInputFocus::Unit;
	}
};