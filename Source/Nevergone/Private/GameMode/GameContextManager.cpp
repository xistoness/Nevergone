// Copyright Xyzto Works


#include "GameMode/GameContextManager.h"
#include "GameMode/TowerFloorGameMode.h"
#include "GameMode/CombatManager.h"
#include "GameMode/BattlePreparationContext.h"

#include "Engine/World.h"
#include "GameMode/BattlefieldLayoutSubsystem.h"
#include "GameMode/BattlefieldQuerySubsystem.h"
#include "Level/EncounterGeneratorSubsystem.h"
#include "Level/EncounterPartySubsystem.h"
#include "Level/FloorEncounterVolume.h"


void UGameContextManager::RequestExploration()
{
	if (!CanEnterState(EGameContextState::Exploration))
		return;

	EnterState(EGameContextState::Exploration);
}

void UGameContextManager::RequestBattlePreparation(class AFloorEncounterVolume* EncounterSource)
{
	if (!CanEnterState(EGameContextState::BattlePreparation))
		return;

	EnterState(EGameContextState::BattlePreparation);
	
	ActiveBattlePrepContext = NewObject<UBattlePreparationContext>(this);
	
	CreateCombatManager();
	
	// 1. Generate encounter data (enemy party only, no spawn)
	auto* EncounterGen = GetWorld()->GetSubsystem<UEncounterGeneratorSubsystem>();
	EncounterGen->GenerateEncounter(
		EncounterSource->EncounterData,
		*ActiveBattlePrepContext
	);
	
	// 2. Generate party data (player party only, no spawn)
	auto* PartyGen = GetWorld()->GetSubsystem<UEncounterPartySubsystem>();
	PartyGen->GeneratePlayerParty(
		EncounterSource->EncounterData,
		*ActiveBattlePrepContext
	);

	// 3. Plan battlefield layout
	auto* BattlefieldQuery = GetWorld()->GetSubsystem<UBattlefieldQuerySubsystem>();
	BattlefieldQuery->BuildCache();
	
	auto* Layout = GetWorld()->GetSubsystem<UBattlefieldLayoutSubsystem>();
	Layout->PlanSpawns(
		EncounterSource,
		*ActiveBattlePrepContext
	);

	// 4. Prepare combat manager (no spawn)
	ActiveCombatManager->EnterPreparation(*ActiveBattlePrepContext);

	// 5. Open UI
	// (UI queries ActiveBattleContext only)
}

void UGameContextManager::AbortBattle()
{
	ActiveCombatManager->CancelPreparation();
	ActiveBattlePrepContext = nullptr;
}

void UGameContextManager::RequestBattleStart()
{
	if (!CanEnterState(EGameContextState::Battle))
		return;

	if (!ActiveBattlePrepContext ||
		!ActiveBattlePrepContext->IsReadyToStartCombat())
	{
		return;
	}

	EnterState(EGameContextState::Battle);

	check(ActiveCombatManager);

	ActiveCombatManager->StartCombat(*ActiveBattlePrepContext);

	// Context is now immutable and discarded
	ActiveBattlePrepContext = nullptr;
}

void UGameContextManager::RequestBattleEnd()
{
	if (ActiveCombatManager)
	{
		ActiveCombatManager->EndCombat();
	}
}

void UGameContextManager::RequestTransition()
{
	EnterState(EGameContextState::Transition);
}

bool UGameContextManager::CanEnterState(EGameContextState TargetState) const
{
	const ATowerFloorGameMode* FloorGM = GetActiveFloorGameMode();
	if (!FloorGM)
		return false;

	switch (TargetState)
	{
	case EGameContextState::Exploration:
		return FloorGM->AllowsExploration();

	case EGameContextState::BattlePreparation:
	case EGameContextState::Battle:
		return FloorGM->AllowsCombat();

	default:
		return true;
	}
}

void UGameContextManager::EnterState(EGameContextState NewState)
{
	ExitState(CurrentState);
	CurrentState = NewState;

	// Create / enable systems here (CombatManager, UI, input, etc)
}

void UGameContextManager::ExitState(EGameContextState OldState)
{
	// Destroy / disable systems related to OldState
}

void UGameContextManager::CreateCombatManager()
{
	DestroyCombatManager();

	ActiveCombatManager = NewObject<UCombatManager>(this);
	ActiveCombatManager->Initialize();
	ActiveCombatManager->OnCombatFinished.AddUObject(
		this, &UGameContextManager::HandleCombatFinished
	);
}

void UGameContextManager::DestroyCombatManager()
{
	if (!ActiveCombatManager)
		return;

	ActiveCombatManager->OnCombatFinished.Clear();
	ActiveCombatManager = nullptr;
}

void UGameContextManager::HandleCombatFinished()
{
	DestroyCombatManager();

	// Decide next state (simplified)
	RequestExploration();
}

ATowerFloorGameMode* UGameContextManager::GetActiveFloorGameMode() const
{
	if (!GetWorld())
		return nullptr;

	return GetWorld()->GetAuthGameMode<ATowerFloorGameMode>();
}
