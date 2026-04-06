// Copyright Xyzto Works

#include "Widgets/Combat/BattlePreparationWidget.h"

#include "ActorComponents/UnitStatsComponent.h"
#include "Audio/AudioSubsystem.h"
#include "Characters/CharacterBase.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Data/UnitDefinition.h"
#include "GameMode/BattlePreparationContext.h"
#include "Types/BattleTypes.h"

// ---------------------------------------------------------------------------
// UMG lifecycle
// ---------------------------------------------------------------------------

class UAudioSubsystem;

void UBattlePreparationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (StartBattleButton)
	{
		StartBattleButton->OnClicked.AddDynamic(
			this, &UBattlePreparationWidget::HandleStartBattleClicked);
	}
	if (AbortBattleButton)
	{
		AbortBattleButton->OnClicked.AddDynamic(
			this, &UBattlePreparationWidget::HandleAbortBattleClicked);
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UBattlePreparationWidget::PopulateFromContext(UBattlePreparationContext* Context)
{
	if (!Context)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[BattlePreparationWidget] PopulateFromContext: null context — widget will be empty"));
		return;
	}

	// --- Enemy section ---

	const int32 EnemyCount = Context->EnemyParty.Num();

	if (EnemyCountText)
	{
		EnemyCountText->SetText(
			FText::FromString(FString::Printf(TEXT("Enemies: %d"), EnemyCount)));
	}

	if (EnemyListBox)
	{
		EnemyListBox->ClearChildren();

		for (const FGeneratedEnemyData& EnemyData : Context->EnemyParty)
		{
			// Resolve DisplayName from the unit class CDO.
			// The CDO (Class Default Object) is a read-only default instance of the class
			// that UE keeps in memory — we use it here to read the UnitDefinition without
			// spawning the actor, which would be expensive just to read a name.
			FText DisplayName = FText::FromString(TEXT("Unknown"));

			if (EnemyData.EnemyClass)
			{
				const ACharacterBase* CDO = EnemyData.EnemyClass->GetDefaultObject<ACharacterBase>();
				if (CDO)
				{
					const UUnitStatsComponent* Stats = CDO->GetUnitStats();
					if (Stats && Stats->GetDefinition())
					{
						DisplayName = Stats->GetDefinition()->DisplayName;
					}
				}
			}

			AddUnitRow(EnemyListBox, DisplayName, EnemyData.Level);
		}
	}

	// --- Ally section ---

	const int32 AllyCount = Context->PlayerParty.Num();

	if (AllyCountText)
	{
		AllyCountText->SetText(
			FText::FromString(FString::Printf(TEXT("Your Party: %d"), AllyCount)));
	}

	if (AllyListBox)
	{
		AllyListBox->ClearChildren();

		for (const FGeneratedPlayerData& PlayerData : Context->PlayerParty)
		{
			FText DisplayName = FText::FromString(TEXT("Unknown"));

			if (PlayerData.PlayerClass)
			{
				const ACharacterBase* CDO = PlayerData.PlayerClass->GetDefaultObject<ACharacterBase>();
				if (CDO)
				{
					const UUnitStatsComponent* Stats = CDO->GetUnitStats();
					if (Stats && Stats->GetDefinition())
					{
						DisplayName = Stats->GetDefinition()->DisplayName;
					}
				}
			}

			AddUnitRow(AllyListBox, DisplayName, PlayerData.Level);
		}
	}

	// Blueprint can react to this event to trigger entry animations, etc.
	OnPreparationReady();

	UE_LOG(LogTemp, Log,
		TEXT("[BattlePreparationWidget] Populated — Enemies: %d | Allies: %d"),
		EnemyCount, AllyCount);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UBattlePreparationWidget::AddUnitRow(UVerticalBox* TargetBox, const FText& DisplayName, int32 Level)
{
	if (!TargetBox) { return; }

	// Create a plain TextBlock as a row. The Blueprint subclass can override the
	// widget style by either: (a) replacing the row widget class via a virtual,
	// or (b) styling the parent VerticalBox uniformly. For Pass 1 this is enough.
	UTextBlock* Row = NewObject<UTextBlock>(this);
	if (!Row) { return; }

	const FString RowString = FString::Printf(TEXT("%s  Lv.%d"), *DisplayName.ToString(), Level);
	Row->SetText(FText::FromString(RowString));

	TargetBox->AddChildToVerticalBox(Row);
}

// ---------------------------------------------------------------------------
// Button handler
// ---------------------------------------------------------------------------

void UBattlePreparationWidget::HandleStartBattleClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[BattlePreparationWidget] Start Battle clicked"));

	OnStartBattleClicked.Broadcast();
	
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
		{
			Audio->PlayUISoundEvent(EUISoundEvent::ButtonConfirm);
		}
	}

	// The controller drives the actual transition — the widget just fires the delegate.
	RemoveFromParent();
}

void UBattlePreparationWidget::HandleAbortBattleClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[BattlePreparationWidget] Start Battle clicked"));

	OnAbortBattleClicked.Broadcast();
	
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>())
		{
			Audio->PlayUISoundEvent(EUISoundEvent::ButtonCancel);
		}
	}

	// The controller drives the actual transition — the widget just fires the delegate.
	RemoveFromParent();
}