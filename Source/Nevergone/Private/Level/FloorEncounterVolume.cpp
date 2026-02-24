// Copyright Xyzto Works


#include "Level/FloorEncounterVolume.h"
#include "Level/BattleVolume.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameInstance/MyGameInstance.h"
#include "GameInstance/GameContextManager.h"
#include "Engine/World.h"

AFloorEncounterVolume::AFloorEncounterVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	// --- Collision setup ---
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
}

ABattleVolume* AFloorEncounterVolume::GetBattleVolume() const
{
	return BattleVolume;
}

void AFloorEncounterVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bAutoStartOnEnter || !EncounterData)
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
}
