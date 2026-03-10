// Copyright Xyzto Works

#include "Level/LevelPortal.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ArrowComponent.h"
#include "GameInstance/MyGameInstance.h"
#include "Kismet/GameplayStatics.h"

ALevelPortal::ALevelPortal()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->SetCollisionProfileName(TEXT("Trigger"));

	SpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(Root);

	SpawnArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnArrow"));
	SpawnArrow->SetupAttachment(SpawnPoint);

	// Optional: keep the actor hidden in game, but visible in editor
	// SetActorHiddenInGame(true);
}

void ALevelPortal::BeginPlay()
{
	Super::BeginPlay();

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ALevelPortal::OnOverlapBegin);
}

FTransform ALevelPortal::GetEntryTransform() const
{
	// SpawnPoint is the authoritative visual marker for player placement
	return SpawnPoint ? SpawnPoint->GetComponentTransform() : GetActorTransform();
}

void ALevelPortal::OnOverlapBegin(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	UMyGameInstance* GI = GetGameInstance<UMyGameInstance>();
	if (!GI)
	{
		return;
	}

	if (TargetLevel.IsNone() || TargetPortalID.IsNone())
	{
		return;
	}

	FLevelTransitionContext Context;
	Context.FromLevel = GetWorld()->GetOutermost()->GetFName();
	Context.ToLevel = TargetLevel;
	Context.EntryPortalID = TargetPortalID;

	GI->RequestLevelChange(TargetLevel, Context);
}