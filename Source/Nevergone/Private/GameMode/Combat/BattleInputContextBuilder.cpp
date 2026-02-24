// Copyright Xyzto Works


#include "GameMode/Combat/BattleInputContextBuilder.h"

#include "GameMode/TurnManager.h"

FBattleInputContext UBattleInputContextBuilder::BuildContext() const
{
	FBattleInputContext Context;

	// Turn ownership
	Context.TurnOwner = TurnManager
		? TurnManager->GetCurrentTurnOwner()
		: EBattleTurnOwner::System;

	// Turn phase
	Context.TurnPhase = TurnManager
		? TurnManager->GetCurrentTurnPhase()
		: EBattleTurnPhase::Transition;

	// Interaction
	Context.InteractionMode = InteractionMode;

	// Input focus resolution
	if (bHardLock)
	{
		Context.InputFocus = EBattleInputFocus::None;
		Context.bInputLocked = true;
	}
	else if (bUIOpen)
	{
		Context.InputFocus = EBattleInputFocus::UI;
	}
	else if (InteractionMode == EBattleInteractionMode::Targeting)
	{
		Context.InputFocus = EBattleInputFocus::Unit;
	}
	else if (bCameraEnabled)
	{
		Context.bCameraInputEnabled = bCameraEnabled;
	}

	return Context;
}

void UBattleInputContextBuilder::SetTurnManager(UTurnManager* InTurnManager)
{
	TurnManager = InTurnManager;
}

void UBattleInputContextBuilder::SetUIState(bool bIsUIOpen)
{
	bUIOpen = bIsUIOpen;
}

void UBattleInputContextBuilder::SetInteractionMode(EBattleInteractionMode Mode)
{
	InteractionMode = Mode;
}

void UBattleInputContextBuilder::SetCameraInputEnabled(bool bEnabled)
{
	bCameraEnabled = bEnabled;
}

void UBattleInputContextBuilder::SetHardLock(bool bLocked)
{
	bHardLock = bLocked;
}