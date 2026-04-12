// Copyright Xyzto Works

#include "Level/FloorEncounterVolume.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/ActorSaveData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameInstance/MyGameInstance.h"
#include "GameInstance/GameContextManager.h"
#include "GameInstance/SaveKeys.h"
#include "Engine/World.h"

AFloorEncounterVolume::AFloorEncounterVolume()
{
	// Tick is needed for the bob + rotation animation.
	// Disabled when the encounter is resolved so idle encounters cost nothing.
	PrimaryActorTick.bCanEverTick = true;

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
	BattleBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BattleBox"));
	BattleBox->SetupAttachment(RootComponent);
	BattleBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BattleBox->SetBoxExtent(FVector(500.f, 500.f, 50.f));
	BattleBox->SetLineThickness(3.f);

	// --- Indicator mesh (replaces the old SkeletalMeshComponent) ---
	// Assign a static mesh in the Blueprint CDO or in the viewport.
	// The bob + rotation are driven purely by Tick — no animation asset needed.
	IndicatorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IndicatorMesh"));
	IndicatorMesh->SetupAttachment(RootComponent);
	IndicatorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IndicatorMesh->SetGenerateOverlapEvents(false);

	SaveableComponent = CreateDefaultSubobject<USaveableComponent>(TEXT("SaveableComponent"));
}

void AFloorEncounterVolume::BeginPlay()
{
	Super::BeginPlay();

	if (IndicatorMesh)
	{
		// Snapshot the mesh's designed Z so the bob is a clean delta on top of it.
		MeshBaseZ = IndicatorMesh->GetRelativeLocation().Z;
	}

}

void AFloorEncounterVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IndicatorMesh) { return; }

	// --- Bob (vertical sine wave) ---
	// Wrap BobTime at 2PI to prevent float precision loss over very long sessions.
	BobTime += DeltaTime * BobFrequency * 2.f * PI;
	if (BobTime > 2.f * PI) { BobTime -= 2.f * PI; }

	const float BobOffset = FMath::Sin(BobTime) * (BobAmplitude * 0.5f);

	FVector RelLoc = IndicatorMesh->GetRelativeLocation();
	RelLoc.Z = MeshBaseZ + BobOffset;
	IndicatorMesh->SetRelativeLocation(RelLoc);

	// --- Rotation (slow spin around local Z) ---
	const float DeltaYaw = RotationSpeed * DeltaTime;
	IndicatorMesh->AddLocalRotation(FRotator(0.f, DeltaYaw, 0.f));
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
	bool TempIsInBattle = bIsInBattle;
	Ar << TempIsInBattle;

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
	Ar << bIsInBattle;
}

void AFloorEncounterVolume::OnPostRestore_Implementation()
{
	if (bEncounterResolved)
	{
		DeactivateEncounter();
	}
	if (bIsInBattle)
	{
		IndicatorMesh->SetVisibility(false);
	}
}

void AFloorEncounterVolume::RestoreEncounterVisual()
{
	if (IndicatorMesh && !bEncounterResolved)
	{
		bIsInBattle = false;
		IndicatorMesh->SetVisibility(true);
	}
}

// --- Encounter lifecycle ---

void AFloorEncounterVolume::DeactivateEncounter()
{
	bEncounterResolved = true;

	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	// Stop ticking — no visual to animate when the encounter is gone.
	SetActorTickEnabled(false);

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

	// Hide the indicator immediately so there's no visual pop when the
	// battle starts and the actor is later fully deactivated.
	if (IndicatorMesh)
	{
		bIsInBattle = true;
		IndicatorMesh->SetVisibility(false);
	}

	ContextManager->RequestBattlePreparation(this);
}