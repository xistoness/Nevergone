// Copyright Xyzto Works


#include "Actors/KnockableProp.h"

#include "Nevergone.h"
#include "Characters/ExcursionSurvivorNPC.h"
#include "EngineTools/CollisionChannels.h"
#include "World/ExcursionNoiseSubsystem.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

AKnockableProp::AKnockableProp()
{
	// Ticks only while a survivor is inside the trigger.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	PropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropMesh"));
	SetRootComponent(PropMesh);

	PropMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PropMesh->SetCollisionObjectType(ECC_PhysicsBody);
	PropMesh->SetCollisionResponseToAllChannels(ECR_Block);
	PropMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// A bottle does not muffle sound. Without this, a prop lying between an
	// enemy and a noise would count as a wall.
	PropMesh->SetCollisionResponseToChannel(NevergoneCollision::NoiseOcclusion, ECR_Ignore);

	PropMesh->SetSimulatePhysics(false);
	PropMesh->SetNotifyRigidBodyCollision(true);

	BumpTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("BumpTrigger"));
	BumpTrigger->SetupAttachment(PropMesh);
	BumpTrigger->SetBoxExtent(FVector(30.0f, 30.0f, 50.0f));
	BumpTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BumpTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	BumpTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BumpTrigger->SetGenerateOverlapEvents(true);
}

void AKnockableProp::BeginPlay()
{
	Super::BeginPlay();

	BumpTrigger->OnComponentBeginOverlap.AddDynamic(this, &AKnockableProp::HandleBumpBegin);
	BumpTrigger->OnComponentEndOverlap.AddDynamic(this, &AKnockableProp::HandleBumpEnd);

	PropMesh->OnComponentHit.AddDynamic(this, &AKnockableProp::HandlePropHit);

	if (!ImpactNoise)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[KnockableProp] %s has no ImpactNoise; it will fall silently"),
			*GetNameSafe(this));
	}
}

void AKnockableProp::HandleBumpBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);

	if (!Pawn || !AExcursionSurvivorNPC::IsSurvivorBody(Pawn))
	{
		return;
	}

	// Entering the trigger is what arms the prop for this survivor.
	ArmedByOverlappingSurvivor.Add(Pawn, true);

	EvaluateBump();

	// The survivor may still speed up without leaving the trigger.
	SetActorTickEnabled(true);
}

void AKnockableProp::HandleBumpEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APawn* Pawn = Cast<APawn>(OtherActor);

	if (!Pawn || ArmedByOverlappingSurvivor.Remove(Pawn) == 0)
	{
		return;
	}

	// Leaving is what lets the next approach knock the prop again.
	if (ArmedByOverlappingSurvivor.IsEmpty())
	{
		SetActorTickEnabled(false);
	}
}

void AKnockableProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	EvaluateBump();
}

void AKnockableProp::EvaluateBump()
{
	for (auto It = ArmedByOverlappingSurvivor.CreateIterator(); It; ++It)
	{
		APawn* Survivor = It.Key().Get();

		if (!Survivor)
		{
			It.RemoveCurrent();

			continue;
		}

		if (!It.Value() || Survivor->GetVelocity().Size2D() < KnockSpeedThreshold)
		{
			continue;
		}

		// Disarmed for this survivor until it steps out of the trigger, so
		// walking into the prop and staying there does not kick it every
		// frame.
		It.Value() = false;

		Knock(Survivor);

		// One knock per evaluation; a second fast survivor gets the next tick.
		return;
	}

	if (ArmedByOverlappingSurvivor.IsEmpty())
	{
		SetActorTickEnabled(false);
	}
}

void AKnockableProp::Knock(APawn* ByPawn)
{
	KnockedBy = ByPawn;

	if (!bPhysicsEnabled)
	{
		bPhysicsEnabled = true;

		PropMesh->SetSimulatePhysics(true);
	}

	// Push away from the survivor, horizontally. Falls back to the survivor's
	// facing when both pivots line up.
	FVector Direction = GetActorLocation() - ByPawn->GetActorLocation();
	Direction.Z = 0.0f;

	if (!Direction.Normalize())
	{
		Direction = ByPawn->GetActorForwardVector();
	}

	PropMesh->AddImpulse(Direction * KnockVelocity, NAME_None, /*bVelChange=*/true);

	UE_LOG(LogNevergone, Log,
		TEXT("[KnockableProp] %s knocked by %s at %.0f cm/s"),
		*GetNameSafe(this), *GetNameSafe(ByPawn), ByPawn->GetVelocity().Size2D());
}

void AKnockableProp::HandlePropHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bPhysicsEnabled)
	{
		return;
	}

	const UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	// Impulse is mass times velocity change, so dividing by the mass gives
	// how hard the impact was regardless of how heavy the prop is.
	const float Mass = FMath::Max(PropMesh->GetMass(), KINDA_SMALL_NUMBER);

	const float ImpactSpeed = NormalImpulse.Size() / Mass;

	if (ImpactSpeed < MinImpactSpeed)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();

	if (Now - LastNoiseTime < MinNoiseIntervalSeconds)
	{
		return;
	}

	LastNoiseTime = Now;

	if (UExcursionNoiseSubsystem* Noise = UExcursionNoiseSubsystem::Get(this))
	{
		Noise->EmitNoise(ImpactNoise, Hit.ImpactPoint, KnockedBy.Get(), Hit.PhysMaterial.Get());
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[KnockableProp] %s hit %s at %.0f cm/s"),
		*GetNameSafe(this), *GetNameSafe(OtherActor), ImpactSpeed);
}
