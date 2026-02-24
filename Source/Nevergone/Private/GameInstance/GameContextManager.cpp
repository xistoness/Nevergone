// Copyright Xyzto Works


#include "GameInstance/GameContextManager.h"

#include "Characters/CharacterBase.h"
#include "GameMode/TowerFloorGameMode.h"
#include "GameMode/CombatManager.h"
#include "GameMode/BattlePreparationContext.h"
#include "GameMode/BattlefieldLayoutSubsystem.h"
#include "GameMode/BattlefieldQuerySubsystem.h"
#include "Level/EncounterGeneratorSubsystem.h"
#include "Level/FloorEncounterVolume.h"
#include "Level/GridManager.h"
#include "Party/PartyManagerSubsystem.h"

#include "Engine/World.h"



void UGameContextManager::RequestExploration()
{
	if (!CanEnterState(EGameContextState::Exploration))
		return;

	EnterState(EGameContextState::Exploration);
}

void UGameContextManager::RequestBattlePreparation(
	AFloorEncounterVolume* EncounterSource)
{
	UE_LOG(LogTemp, Warning, TEXT("[GameContextManager]: Requesting battle preparation..."));

	if (!CanEnterState(EGameContextState::BattlePreparation))
		return;

	EnterState(EGameContextState::BattlePreparation);

	// Create grid manager (empty, not initialized yet)
	UGridManager* Grid = GetWorld()->GetSubsystem<UGridManager>();

	// Create battle preparation context
	ActiveBattlePrepContext = NewObject<UBattlePreparationContext>(this);
	ActiveBattlePrepContext->Initialize();

	// Create combat manager
	CreateCombatManager();

	// 1. Generate encounter data
	auto* EncounterGen = GetWorld()->GetSubsystem<UEncounterGeneratorSubsystem>();
	EncounterGen->GenerateEncounter(
		EncounterSource->EncounterData,
		*ActiveBattlePrepContext
	);

	// 2. Generate player party
	auto* PartyManager =
		GetGameInstance()->GetSubsystem<UPartyManagerSubsystem>();

	PartyManager->BuildBattleParty(
		EncounterSource->EncounterData,
		ActiveBattlePrepContext->PlayerParty
	);

	// 3. Generate tactical grid
	if (ABattleVolume* BattleVolume =
		EncounterSource->GetBattleVolume())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameContextManager]: Tries to generate grid..."));
		GetWorld()->GetSubsystem<UGridManager>()->GenerateGrid(BattleVolume, GetWorld());
	}

	// 4. Plan battlefield layout
	auto* BattlefieldQuery =
		GetWorld()->GetSubsystem<UBattlefieldQuerySubsystem>();

	BattlefieldQuery->BuildCache();

	auto* Layout =
		GetWorld()->GetSubsystem<UBattlefieldLayoutSubsystem>();

	Layout->PlanSpawns(
		*BattlefieldQuery,
		*ActiveBattlePrepContext
	);

	// 5. Enter preparation phase
	ActiveCombatManager->EnterPreparation(*ActiveBattlePrepContext);

	UE_LOG(LogTemp, Warning, TEXT("[GameContextManager]: Battle preparation ready!"));

	// Debug auto-start
	RequestBattleStart();
}

void UGameContextManager::AbortBattle()
{
	ActiveCombatManager->CancelPreparation();
	ActiveBattlePrepContext = nullptr;
}

void UGameContextManager::RequestBattleStart()
{
	UE_LOG(LogTemp, Warning, TEXT("[GameContextManager]: Requesting battle start..."));
	if (!CanEnterState(EGameContextState::Battle))
		return;
	UE_LOG(LogTemp, Warning, TEXT("[GameContextManager]: Can enter battle state!"));
	if (!ActiveBattlePrepContext ||
		!ActiveBattlePrepContext->IsReadyToStartCombat())
	{
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[GameContextManager]: Battle preparation context is ready to start combat!"));

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
	if (CurrentState == NewState)
	{
		return;
	}

	const EGameContextState OldState = CurrentState;

	ExitState(OldState);
	CurrentState = NewState;

	OnGameContextChanged.Broadcast(NewState);
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
