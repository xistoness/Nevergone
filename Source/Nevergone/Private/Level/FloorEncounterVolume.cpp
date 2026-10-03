// Copyright Xyzto Works

#include "Level/FloorEncounterVolume.h"

#include "ActorComponents/CombatPresenceComponent.h"
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

	// EndOverlap subscription — required to clear the re-trigger guard so
	// that the encounter can fire again once the player leaves and re-enters
	// the trigger box. See bWaitingForExitBeforeRetrigger doc comment.
	TriggerBox->OnComponentEndOverlap.AddDynamic(
		this,
		&AFloorEncounterVolume::OnTriggerEndOverlap
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

	// Level-placed actor: state is saved while alive (bEncounterResolved, bIsInBattle)
	// but the actor itself is NEVER respawned by SpawnMissingActors — it lives in
	// the level package and is loaded by the engine. Setting this to false prevents
	// duplicate instances from being spawned by the save system if the world-actor
	// matching ever fails.
	SaveableComponent->bPersistAcrossLoads = false;

	// --- Combat presence ---
	// Encounter volumes (active and inactive) are hidden during the combat span.
	// The active one's IndicatorMesh is already hidden by OnTriggerBeginOverlap;
	// HideAndPause additionally cuts the actor's tick (the bob/rotation work is
	// pointless on a hidden mesh) and disables the trigger so the battle camera
	// pawn cannot accidentally re-fire it.
	auto* CombatPresence = CreateDefaultSubobject<UCombatPresenceComponent>(TEXT("CombatPresenceComponent"));
	CombatPresence->DefaultPolicy = ECombatPresencePolicy::HideAndPause;
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
	UE_LOG(LogTemp, Log,
		TEXT("[FloorEncounterVolume] '%s' WriteSaveData: bEncounterResolved=%d bIsInBattle=%d"),
		*GetName(), (int32)bEncounterResolved, (int32)bIsInBattle);

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
	const FSavePayload* Payload = InData.CustomDataMap.Find(SaveKeys::EncounterVolume);
	if (!Payload)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[FloorEncounterVolume] '%s' ReadSaveData: no payload for key '%s' — keeping defaults."),
			*GetName(), *SaveKeys::EncounterVolume.ToString());
		return;
	}

	FMemoryReader Reader(Payload->Data, true);
	FArchive& Ar = Reader;

	Ar << bEncounterResolved;
	Ar << bIsInBattle;

	UE_LOG(LogTemp, Log,
		TEXT("[FloorEncounterVolume] '%s' ReadSaveData: bEncounterResolved=%d bIsInBattle=%d"),
		*GetName(), (int32)bEncounterResolved, (int32)bIsInBattle);
}

void AFloorEncounterVolume::OnPostRestore_Implementation()
{
	if (bEncounterResolved)
	{
		DeactivateEncounter();
	}
	if (bIsInBattle && IndicatorMesh)
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

	// Re-trigger guard: when prep is canceled, the player pawn is destroyed
	// and respawned at the same position, which re-overlaps this trigger box.
	// Without this flag we would re-enter BattlePreparation immediately on
	// respawn, leaving the player unable to abort.
	bWaitingForExitBeforeRetrigger = true;

	UE_LOG(LogTemp, Log,
		TEXT("[FloorEncounterVolume] '%s' RestoreEncounterVisual: bWaitingForExitBeforeRetrigger=true (player must leave & re-enter)."),
		*GetName());
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

	// Authority hand-off: this method is called inside the combat span (on
	// victory) and writes the actor's authoritative end-state. Tell the
	// presence component to drop its snapshot so ExitCombatPresenceMode does
	// NOT restore the pre-combat (visible, ticking) state on top of this.
	if (auto* CombatPresence = FindComponentByClass<UCombatPresenceComponent>())
	{
		CombatPresence->InvalidateSnapshot();
	}
}

void AFloorEncounterVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Re-trigger guard. The player pawn has been respawned inside this
	// trigger box (e.g. right after aborting a battle preparation). Wait
	// until the player physically leaves before allowing a new trigger.
	if (bWaitingForExitBeforeRetrigger)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[FloorEncounterVolume] '%s' BeginOverlap suppressed — waiting for exit-before-retrigger."),
			*GetName());
		return;
	}

	if (!bAutoStartOnEnter || !EncounterData || bEncounterResolved)
	{
		return;
	}

	// Already in battle: BeginOverlap firing here would cause recursive prep.
	// This can happen if, during the combat span, some pawn (e.g. battle
	// camera) overlaps the trigger.
	if (bIsInBattle)
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

	UE_LOG(LogTemp, Log,
		TEXT("[FloorEncounterVolume] '%s' BeginOverlap fired by '%s' — requesting battle preparation."),
		*GetName(), *GetNameSafe(OtherActor));

	ContextManager->RequestBattlePreparation(this);
}

void AFloorEncounterVolume::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// Only react to the player pawn leaving — other pawns (NPCs, battle
	// camera) are irrelevant to the re-trigger guard.
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	if (bWaitingForExitBeforeRetrigger)
	{
		bWaitingForExitBeforeRetrigger = false;

		UE_LOG(LogTemp, Log,
			TEXT("[FloorEncounterVolume] '%s' EndOverlap by player — re-trigger guard cleared."),
			*GetName());
	}
}
