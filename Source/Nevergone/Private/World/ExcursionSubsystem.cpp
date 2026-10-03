// Copyright Xyzto Works

#include "World/ExcursionSubsystem.h"

#include "Nevergone.h"
#include "Characters/ExcursionSurvivorNPC.h"
#include "Data/ItemDefinition.h"
#include "World/ExcursionEncounterSubsystem.h"
#include "World/InventorySubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	using FItemCounts = TMap<TObjectPtr<UItemDefinition>, int32>;

	FExcursionItemCount MakeItemCount(UItemDefinition* Item, int32 Quantity)
	{
		FExcursionItemCount Count;
		Count.Item = Item;
		Count.Quantity = Quantity;

		return Count;
	}

	// Splits the change between two counts into what was gained and what was
	// lost, per item type.
	void DiffItemCounts(const FItemCounts& Before, const FItemCounts& After,
		TArray<FExcursionItemCount>& OutGained, TArray<FExcursionItemCount>& OutLost)
	{
		for (const TPair<TObjectPtr<UItemDefinition>, int32>& Pair : After)
		{
			const int32 Delta = Pair.Value - Before.FindRef(Pair.Key);

			if (Delta > 0)
			{
				OutGained.Add(MakeItemCount(Pair.Key, Delta));
			}
		}

		for (const TPair<TObjectPtr<UItemDefinition>, int32>& Pair : Before)
		{
			const int32 Delta = Pair.Value - After.FindRef(Pair.Key);

			if (Delta > 0)
			{
				OutLost.Add(MakeItemCount(Pair.Key, Delta));
			}
		}
	}

	UInventorySubsystem* GetInventory(const UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

		return GameInstance ? GameInstance->GetSubsystem<UInventorySubsystem>() : nullptr;
	}
}

UExcursionSubsystem* UExcursionSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;

	return World ? World->GetSubsystem<UExcursionSubsystem>() : nullptr;
}

bool UExcursionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UExcursionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	EntryInventory = CountInventory();

	if (UExcursionEncounterSubsystem* Encounter = InWorld.GetSubsystem<UExcursionEncounterSubsystem>())
	{
		Encounter->OnCaptureResolved.AddUObject(this, &UExcursionSubsystem::HandleCaptureResolved);

		EncounterSubsystem = Encounter;
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionSubsystem] %s: excursion started with %d item types in the inventory"),
		*GetNameSafe(&InWorld), EntryInventory.Num());
}

void UExcursionSubsystem::Deinitialize()
{
	if (UExcursionEncounterSubsystem* Encounter = EncounterSubsystem.Get())
	{
		Encounter->OnCaptureResolved.RemoveAll(this);
	}

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// End
// ---------------------------------------------------------------------------

void UExcursionSubsystem::ExitExcursion()
{
	EndExcursion(EExcursionEndReason::Exited);
}

void UExcursionSubsystem::HandleCaptureResolved(ACharacter* Victim, AExplorationEnemyCharacter* Enemy,
	EExcursionCaptureOutcome Outcome)
{
	// A follower's death is already on the follower; only the player's
	// survivor shapes the result.
	if (!Victim || Victim->IsA<AExcursionSurvivorNPC>())
	{
		return;
	}

	switch (Outcome)
	{
	case EExcursionCaptureOutcome::Unharmed:
		++SafeguardsUsed;
		break;

	case EExcursionCaptureOutcome::Infected:
		++BitesTaken;
		break;

	case EExcursionCaptureOutcome::Killed:
		EndExcursion(EExcursionEndReason::SurvivorKilled);
		break;

	default:
		break;
	}
}

void UExcursionSubsystem::EndExcursion(EExcursionEndReason Reason)
{
	if (bEnded)
	{
		return;
	}

	bEnded = true;

	Result = FExcursionResult();
	Result.EndReason = Reason;
	Result.SafeguardsUsed = SafeguardsUsed;

	if (Reason == EExcursionEndReason::SurvivorKilled)
	{
		Result.Condition = EExcursionSurvivorCondition::Dead;
	}
	else
	{
		Result.Condition = BitesTaken > 0 ? EExcursionSurvivorCondition::Infected : EExcursionSurvivorCondition::Unharmed;
	}

	if (Reason == EExcursionEndReason::Exited)
	{
		for (TActorIterator<AExcursionSurvivorNPC> It(GetWorld()); It; ++It)
		{
			if (It->GetSurvivorState() == EExcursionSurvivorState::Following)
			{
				Result.RescuedSurvivors.Add(It->GetDisplayName());

				UE_LOG(LogNevergone, Log,
					TEXT("[ExcursionSubsystem] %s rescued"), *GetNameSafe(*It));
			}
			else if (It->IsRecruited())
			{
				UE_LOG(LogNevergone, Log,
					TEXT("[ExcursionSubsystem] %s was left waiting inside; not rescued"), *GetNameSafe(*It));
			}
		}
	}

	DiffItemCounts(EntryInventory, CountInventory(), Result.ItemsFound, Result.ItemsSpent);

	// Loot only stays with the group when the survivor walks out.
	if (Reason == EExcursionEndReason::SurvivorKilled)
	{
		RemoveItems(Result.ItemsFound);
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionSubsystem] excursion ended (%s): condition %s, %d rescued, %d item types found%s, %d spent, %d safeguards used"),
		*UEnum::GetValueAsString(Result.EndReason),
		*UEnum::GetValueAsString(Result.Condition),
		Result.RescuedSurvivors.Num(),
		Result.ItemsFound.Num(),
		Reason == EExcursionEndReason::SurvivorKilled ? TEXT(" and lost") : TEXT(""),
		Result.ItemsSpent.Num(),
		Result.SafeguardsUsed);

	OnExcursionEnded.Broadcast(Result);
}

// ---------------------------------------------------------------------------
// Restart
// ---------------------------------------------------------------------------

void UExcursionSubsystem::RestartExcursion()
{
	// Undo every inventory change the excursion made, gains and losses.
	TArray<FExcursionItemCount> Gained;
	TArray<FExcursionItemCount> Lost;

	DiffItemCounts(EntryInventory, CountInventory(), Gained, Lost);

	RemoveItems(Gained);
	AddItems(Lost);

	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionSubsystem] restarting excursion in %s (%d item types restored)"),
		*LevelName, Gained.Num() + Lost.Num());

	// Opened directly instead of through UMyGameInstance::RequestLevelChange:
	// that path autosaves before traveling, and nothing from inside an
	// excursion may be saved. Reopened this way, the level comes back as its
	// last save left it, which is its state at entry.
	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

// ---------------------------------------------------------------------------
// Inventory
// ---------------------------------------------------------------------------

TMap<TObjectPtr<UItemDefinition>, int32> UExcursionSubsystem::CountInventory() const
{
	TMap<TObjectPtr<UItemDefinition>, int32> Counts;

	const UInventorySubsystem* Inventory = GetInventory(GetWorld());

	if (!Inventory)
	{
		return Counts;
	}

	for (const FInventoryEntry& Entry : Inventory->GetInventory())
	{
		if (UItemDefinition* Item = Entry.Definition.LoadSynchronous())
		{
			Counts.FindOrAdd(Item) += Entry.Quantity;
		}
	}

	return Counts;
}

void UExcursionSubsystem::RemoveItems(const TArray<FExcursionItemCount>& Items) const
{
	UInventorySubsystem* Inventory = GetInventory(GetWorld());

	if (!Inventory)
	{
		return;
	}

	for (const FExcursionItemCount& Count : Items)
	{
		if (Inventory->RemoveItem(Count.Item, Count.Quantity) != EInventoryResult::Success)
		{
			UE_LOG(LogNevergone, Warning,
				TEXT("[ExcursionSubsystem] could not remove %s x%d from the inventory"),
				*GetNameSafe(Count.Item.Get()), Count.Quantity);
		}
	}
}

void UExcursionSubsystem::AddItems(const TArray<FExcursionItemCount>& Items) const
{
	UInventorySubsystem* Inventory = GetInventory(GetWorld());

	if (!Inventory)
	{
		return;
	}

	for (const FExcursionItemCount& Count : Items)
	{
		if (Inventory->AddItem(Count.Item, Count.Quantity) != EInventoryResult::Success)
		{
			UE_LOG(LogNevergone, Warning,
				TEXT("[ExcursionSubsystem] could not give back %s x%d to the inventory"),
				*GetNameSafe(Count.Item.Get()), Count.Quantity);
		}
	}
}
