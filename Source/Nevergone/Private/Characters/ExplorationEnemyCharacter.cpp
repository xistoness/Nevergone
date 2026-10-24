// Copyright Xyzto Works


#include "Characters/ExplorationEnemyCharacter.h"

#include "Nevergone.h"
#include "ActorComponents/ExplorationEnemySignalComponent.h"
#include "Data/ExplorationEnemyDefinition.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"

AExplorationEnemyCharacter::AExplorationEnemyCharacter()
{
	// The controller owns the decision loop and ticks; the body does not.
	PrimaryActorTick.bCanEverTick = false;

	// Placed enemies must be possessed in the level, not only when spawned.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->bUseControllerDesiredRotation = false;

		// Zombies never sprint-stop: a long brake distance reads as weight and
		// gives the player a moment to break line of sight after a corner.
		Movement->BrakingDecelerationWalking = 800.0f;
	}

	SignalComponent = CreateDefaultSubobject<UExplorationEnemySignalComponent>(TEXT("Signals"));
}

void AExplorationEnemyCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Applied here rather than in BeginPlay so the capsule and mesh are already
	// correct before the controller possesses the pawn and queries its size.
	ApplyDefinition();
}

void AExplorationEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();

	// The navigation agent has to cover the biggest capsule that walks the navmesh,
	// and Blueprints and the definition can override the C++ default.
	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExplorationEnemyCharacter] %s capsule: radius %.1f, half height %.1f"),
		*GetNameSafe(this),
		GetCapsuleComponent()->GetScaledCapsuleRadius(), GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	if (!Definition)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExplorationEnemyCharacter] %s has no Definition assigned; it will not behave as an enemy"),
			*GetNameSafe(this));
	}
}

void AExplorationEnemyCharacter::ApplyDefinition()
{
	if (!Definition)
	{
		return;
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(Definition->CapsuleRadius, Definition->CapsuleHalfHeight);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = Definition->PatrolSpeed;
		Movement->RotationRate = FRotator(0.0f, Definition->TurnRateDegrees, 0.0f);
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		// Keep the feet on the capsule floor whatever the type's height is.
		// The -90 degree yaw is the engine default that aligns a skeletal mesh
		// authored facing +Y with the capsule's forward axis.
		MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -Definition->CapsuleHalfHeight));
		MeshComponent->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	ApplyVisualVariant();
}

void AExplorationEnemyCharacter::ApplyVisualVariant()
{
	const int32 VariantCount = Definition ? Definition->VisualVariants.Num() : 0;

	if (VariantCount == 0)
	{
		// Not an error: a type may ship a single mesh set on the Blueprint.
		return;
	}

	ActiveVisualVariantIndex = ResolveVisualVariantIndex(VariantCount);

	const FExplorationEnemyVisualVariant& Variant = Definition->VisualVariants[ActiveVisualVariantIndex];

	USkeletalMeshComponent* MeshComponent = GetMesh();

	if (!MeshComponent)
	{
		return;
	}

	if (USkeletalMesh* LoadedMesh = Variant.Mesh.LoadSynchronous())
	{
		MeshComponent->SetSkeletalMeshAsset(LoadedMesh);
	}
	else
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExplorationEnemyCharacter] %s failed to load mesh for visual variant %d"),
			*GetNameSafe(this), ActiveVisualVariantIndex);

		return;
	}

	if (UClass* LoadedAnimClass = Variant.AnimClass.LoadSynchronous())
	{
		MeshComponent->SetAnimInstanceClass(LoadedAnimClass);
	}

	MeshComponent->SetRelativeScale3D(FVector(Variant.UniformScale));

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExplorationEnemyCharacter] %s using visual variant %d of %d"),
		*GetNameSafe(this), ActiveVisualVariantIndex, VariantCount);
}

int32 AExplorationEnemyCharacter::ResolveVisualVariantIndex(int32 VariantCount) const
{
	if (VariantCount <= 1)
	{
		return 0;
	}

	const uint32 Hash = FCrc::StrCrc32(*GetName());

	return static_cast<int32>(Hash % static_cast<uint32>(VariantCount));
}

void AExplorationEnemyCharacter::SetBehaviorState(EExplorationEnemyState NewState)
{
	if (BehaviorState == NewState)
	{
		return;
	}

	const EExplorationEnemyState OldState = BehaviorState;

	BehaviorState = NewState;

	if (Definition)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			const bool bIsPursuing =
				NewState == EExplorationEnemyState::Chase ||
				NewState == EExplorationEnemyState::Catch;

			Movement->MaxWalkSpeed = bIsPursuing ? Definition->ChaseSpeed : Definition->PatrolSpeed;
		}
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExplorationEnemyCharacter] %s state %s -> %s"),
		*GetNameSafe(this),
		*StaticEnum<EExplorationEnemyState>()->GetNameStringByValue(static_cast<int64>(OldState)),
		*StaticEnum<EExplorationEnemyState>()->GetNameStringByValue(static_cast<int64>(NewState)));

	OnStateChanged.Broadcast(OldState, NewState);
}

EExplorationPatrolMode AExplorationEnemyCharacter::GetEffectivePatrolMode() const
{
	if (!Definition)
	{
		return EExplorationPatrolMode::Stationary;
	}

	if (Definition->PatrolMode != EExplorationPatrolMode::Route)
	{
		return Definition->PatrolMode;
	}

	int32 UsableWaypoints = 0;

	for (const TObjectPtr<AActor>& Waypoint : PatrolWaypoints)
	{
		if (IsValid(Waypoint))
		{
			++UsableWaypoints;
		}
	}

	if (UsableWaypoints < 2)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExplorationEnemyCharacter] %s is set to Route patrol but has %d usable waypoints; falling back to Stationary"),
			*GetNameSafe(this), UsableWaypoints);

		return EExplorationPatrolMode::Stationary;
	}

	return EExplorationPatrolMode::Route;
}

float AExplorationEnemyCharacter::GetCatchDistance() const
{
	return Definition ? Definition->CatchDistance : 100.0f;
}

bool AExplorationEnemyCharacter::IsWithinCatchDistance(const AActor* Target) const
{
	if (!IsValid(Target))
	{
		return false;
	}

	const FVector Delta = Target->GetActorLocation() - GetActorLocation();

	const float CatchDistance = GetCatchDistance();

	return Delta.SizeSquared2D() <= FMath::Square(CatchDistance);
}