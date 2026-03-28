// Copyright Xyzto Works


#include "Level/FloorEncounterVolume.h"
#include "Components/BoxComponent.h"
#include "Data/ActorSaveData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameInstance/MyGameInstance.h"
#include "GameInstance/GameContextManager.h"
#include "GameInstance/SaveKeys.h"
#include "Engine/World.h"

AFloorEncounterVolume::AFloorEncounterVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	// --- Trigger box (player detection) ---
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));

	TriggerBox->OnComponentBeginOverlap.AddDynamic(
		this,
		&AFloorEncounterVolume::OnTriggerBeginOverlap
	);

	// --- Battle box (grid area) ---
	// Positioned relative to the root so you can size and move it independently.
	// Default: 1000x1000 footprint — adjust in the viewport per encounter.
	BattleBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BattleBox"));
	BattleBox->SetupAttachment(RootComponent);
	BattleBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BattleBox->SetBoxExtent(FVector(500.f, 500.f, 50.f));
	BattleBox->SetLineThickness(3.f);

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	SkeletalMeshComponent->SetupAttachment(RootComponent);
	
	SaveableComponent = CreateDefaultSubobject<USaveableComponent>(TEXT("SaveableComponent"));
}

// --- Grid config accessors ---

FVector AFloorEncounterVolume::GetBattleBoxOrigin() const
{
	return BattleBox->GetComponentLocation();
}

FVector AFloorEncounterVolume::GetBattleBoxExtent() const
{
	return BattleBox->GetScaledBoxExtent();
}

// --- Save / Restore ---

void AFloorEncounterVolume::WriteSaveData_Implementation(FActorSaveData& OutData) const
{
	UE_LOG(LogTemp, Warning, TEXT("[FloorEncounterVolume] Writing save — bEncounterResolved: %d"), bEncounterResolved);

	FSavePayload Payload;
	FMemoryWriter Writer(Payload.Data, true);
	FArchive& Ar = Writer;

	bool TempResolved = bEncounterResolved;
	Ar << TempResolved;

	OutData.CustomDataMap.Add(SaveKeys::EncounterVolume, MoveTemp(Payload));
}

void AFloorEncounterVolume::ReadSaveData_Implementation(const FActorSaveData& InData)
{
	UE_LOG(LogTemp, Warning, TEXT("[FloorEncounterVolume] Reading save — bEncounterResolved: %d"), bEncounterResolved);

	const FSavePayload* Payload = InData.CustomDataMap.Find(SaveKeys::EncounterVolume);
	if (!Payload) return;

	FMemoryReader Reader(Payload->Data, true);
	FArchive& Ar = Reader;

	Ar << bEncounterResolved;
}

void AFloorEncounterVolume::OnPostRestore_Implementation()
{
	// State is already in bEncounterResolved from ReadSaveData.
	// Apply the visual/collision consequences here, after the world is fully loaded.
	if (bEncounterResolved)
	{
		DeactivateEncounter();
	}
}

// --- Encounter lifecycle ---

void AFloorEncounterVolume::DeactivateEncounter()
{
	bEncounterResolved = true;

	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	bAutoStartOnEnter = false;
}

void AFloorEncounterVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bAutoStartOnEnter || !EncounterData || bEncounterResolved)
	{
		return;
	}

	if (!OtherActor)
	{
		return;
	}

	// Only player-controlled pawns can trigger encounters
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	UGameContextManager* ContextManager =
		GameInstance->GetSubsystem<UGameContextManager>();

	if (!ContextManager)
	{
		return;
	}

	ContextManager->RequestBattlePreparation(this);
	
	if (!SkeletalMeshComponent)
	{
		return;
	}
	SkeletalMeshComponent->SetVisibility(false);
}

