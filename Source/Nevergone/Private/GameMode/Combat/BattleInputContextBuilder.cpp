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
	}
	else if (bUIOpen)
	{
		Context.InputFocus = EBattleInputFocus::UI;
	}
	else if (InteractionMode == EBattleInteractionMode::Targeting)
	{
		Context.InputFocus = EBattleInputFocus::Unit;
	}

	Context.bCameraInputEnabled = bCameraEnabled;
	Context.bUnitInputEnabled = bUnitEnabled;
	
	UE_LOG(LogTemp, Warning, TEXT(
	"[InputContextBuilder] Context Built | TurnOwner=%d | TurnPhase=%d | InteractionMode=%d | InputFocus=%d | CameraInput=%s | UnitInput=%s | HardLock=%s | UIOpen=%s"),
	(int32)Context.TurnOwner,
	(int32)Context.TurnPhase,
	(int32)Context.InteractionMode,
	(int32)Context.InputFocus,
	Context.bCameraInputEnabled ? TEXT("TRUE") : TEXT("FALSE"),
	Context.bUnitInputEnabled ? TEXT("TRUE") : TEXT("FALSE"),
	bHardLock ? TEXT("TRUE") : TEXT("FALSE"),
	bUIOpen ? TEXT("TRUE") : TEXT("FALSE")
);

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

void UBattleInputContextBuilder::SetUnitInputEnabled(bool bEnabled)
{
	bUnitEnabled = bEnabled;
}

void UBattleInputContextBuilder::SetHardLock(bool bLocked)
{
	bHardLock = bLocked;
}

