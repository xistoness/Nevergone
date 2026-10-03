// Copyright Xyzto Works

#include "ActorComponents/ExcursionThrowComponent.h"

#include "Nevergone.h"
#include "Actors/ExcursionThrowable.h"
#include "Characters/PlayerControllers/ExplorationPlayerController.h"
#include "Data/ItemDefinition.h"
#include "World/InventorySubsystem.h"

#include "Animation/AnimMontage.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/GameplayStaticsTypes.h"

#define LOCTEXT_NAMESPACE "ExcursionThrow"

namespace
{
	UInventorySubsystem* GetInventory(const UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

		return GameInstance ? GameInstance->GetSubsystem<UInventorySubsystem>() : nullptr;
	}
}

UExcursionThrowComponent::UExcursionThrowComponent()
{
	// Ticks only while aiming.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UExcursionThrowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAiming();

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Aim
// ---------------------------------------------------------------------------

void UExcursionThrowComponent::BeginAim()
{
	if (bAiming)
	{
		return;
	}

	if (!FindDistraction())
	{
		UE_LOG(LogNevergone, Log,
			TEXT("[ExcursionThrowComponent] %s has nothing to throw"),
			*GetNameSafe(GetOwner()));

		ShowPrompt(LOCTEXT("NothingToThrow", "Nothing to throw"));

		return;
	}

	EnsureArcComponents();

	bAiming = true;

	SetAimRotation(true);

	SetComponentTickEnabled(true);

	UpdateArc();

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExcursionThrowComponent] %s started aiming"),
		*GetNameSafe(GetOwner()));
}

void UExcursionThrowComponent::CancelAim()
{
	if (!bAiming)
	{
		return;
	}

	StopAiming();

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExcursionThrowComponent] %s stopped aiming without throwing"),
		*GetNameSafe(GetOwner()));
}

void UExcursionThrowComponent::ReleaseAim()
{
	if (!bAiming)
	{
		return;
	}

	FVector Start;
	FVector Velocity;

	const bool bCanLaunch = ComputeLaunch(Start, Velocity);

	StopAiming();

	ACharacter* Character = GetCharacter();

	UInventorySubsystem* Inventory = GetInventory(GetWorld());

	UItemDefinition* Item = FindDistraction();

	if (!bCanLaunch || !Character || !Inventory || !Item)
	{
		return;
	}

	if (Inventory->RemoveItem(Item, 1) != EInventoryResult::Success)
	{
		return;
	}

	// Face the throw: the arc may point away from where the body was facing.
	Character->SetActorRotation(FRotator(0.0f, Velocity.Rotation().Yaw, 0.0f));

	if (ThrowMontage)
	{
		Character->PlayAnimMontage(ThrowMontage);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AExcursionThrowable* Thrown = GetWorld()->SpawnActor<AExcursionThrowable>(
		AExcursionThrowable::StaticClass(), Start, Velocity.Rotation(), SpawnParams);

	if (!Thrown)
	{
		// Nothing flew, so the item was never used.
		Inventory->AddItem(Item, 1);

		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionThrowComponent] %s could not spawn the throwable for %s; item returned"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Item));

		return;
	}

	Thrown->Launch(Velocity, Item, Character);

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionThrowComponent] %s threw %s at %.0f cm/s"),
		*GetNameSafe(GetOwner()), *GetNameSafe(Item), Velocity.Size());
}

void UExcursionThrowComponent::StopAiming()
{
	if (!bAiming)
	{
		return;
	}

	bAiming = false;

	SetComponentTickEnabled(false);

	SetAimRotation(false);

	HideArc();
}

void UExcursionThrowComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bAiming)
	{
		UpdateArc();
	}
}

// ---------------------------------------------------------------------------
// Launch and arc
// ---------------------------------------------------------------------------

bool UExcursionThrowComponent::ComputeLaunch(FVector& OutStart, FVector& OutVelocity) const
{
	const ACharacter* Character = GetCharacter();

	const AController* Controller = Character ? Character->GetController() : nullptr;

	if (!Controller)
	{
		return false;
	}

	FRotator Aim = Controller->GetControlRotation();
	Aim.Pitch = FRotator::NormalizeAxis(Aim.Pitch) + AimPitchBiasDegrees;

	OutVelocity = Aim.Vector() * LaunchSpeed;

	const USkeletalMeshComponent* Mesh = Character->GetMesh();

	OutStart = (Mesh && Mesh->DoesSocketExist(ThrowSocketName))
		? Mesh->GetSocketLocation(ThrowSocketName)
		: Character->GetActorTransform().TransformPosition(FallbackThrowOffset);

	return true;
}

void UExcursionThrowComponent::UpdateArc()
{
	FVector Start;
	FVector Velocity;

	if (!ComputeLaunch(Start, Velocity))
	{
		HideArc();

		return;
	}

	FPredictProjectilePathParams Params(AExcursionThrowable::FlightRadius, Start, Velocity, MaxPredictionSeconds);
	Params.bTraceWithCollision = true;
	Params.bTraceWithChannel = false;
	Params.ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	Params.ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	Params.ActorsToIgnore.Add(GetOwner());
	Params.SimFrequency = 15.0f;

	FPredictProjectilePathResult Result;

	UGameplayStatics::PredictProjectilePath(this, Params, Result);

	if (ArcMarkers)
	{
		TArray<FTransform> Transforms;
		Transforms.Reserve(Result.PathData.Num());

		for (const FPredictProjectilePathPointData& Point : Result.PathData)
		{
			Transforms.Add(FTransform(FRotator::ZeroRotator, Point.Location, ArcMarkerScale));
		}

		ArcMarkers->ClearInstances();
		ArcMarkers->AddInstances(Transforms, false, true);
		ArcMarkers->SetVisibility(true);
	}

	if (LandingMarker)
	{
		const FHitResult& Hit = Result.HitResult;

		if (Hit.bBlockingHit)
		{
			LandingMarker->SetWorldLocationAndRotation(Hit.ImpactPoint,
				FRotationMatrix::MakeFromZ(Hit.ImpactNormal).Rotator());

			LandingMarker->SetVisibility(true);
		}
		else
		{
			LandingMarker->SetVisibility(false);
		}
	}
}

void UExcursionThrowComponent::EnsureArcComponents()
{
	AActor* Owner = GetOwner();

	if (!Owner)
	{
		return;
	}

	// World-space components: they draw the arc where it is, whatever the
	// survivor does while aiming.
	if (!ArcMarkers && ArcMarkerMesh)
	{
		ArcMarkers = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("ThrowArcMarkers"));
		ArcMarkers->SetStaticMesh(ArcMarkerMesh);
		ArcMarkers->SetMobility(EComponentMobility::Movable);
		ArcMarkers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ArcMarkers->SetCastShadow(false);
		ArcMarkers->SetUsingAbsoluteLocation(true);
		ArcMarkers->SetUsingAbsoluteRotation(true);
		ArcMarkers->SetUsingAbsoluteScale(true);
		ArcMarkers->SetupAttachment(Owner->GetRootComponent());
		ArcMarkers->RegisterComponent();
		ArcMarkers->SetWorldTransform(FTransform::Identity);
	}

	if (!LandingMarker && LandingMarkerMesh)
	{
		LandingMarker = NewObject<UStaticMeshComponent>(Owner, TEXT("ThrowLandingMarker"));
		LandingMarker->SetStaticMesh(LandingMarkerMesh);
		LandingMarker->SetMobility(EComponentMobility::Movable);
		LandingMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LandingMarker->SetCastShadow(false);
		LandingMarker->SetUsingAbsoluteLocation(true);
		LandingMarker->SetUsingAbsoluteRotation(true);
		LandingMarker->SetupAttachment(Owner->GetRootComponent());
		LandingMarker->RegisterComponent();
	}

	if (!ArcMarkerMesh || !LandingMarkerMesh)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionThrowComponent] %s: ArcMarkerMesh or LandingMarkerMesh not set; the arc will not be fully drawn"),
			*GetNameSafe(Owner));
	}
}

void UExcursionThrowComponent::HideArc()
{
	if (ArcMarkers)
	{
		ArcMarkers->ClearInstances();
		ArcMarkers->SetVisibility(false);
	}

	if (LandingMarker)
	{
		LandingMarker->SetVisibility(false);
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

UItemDefinition* UExcursionThrowComponent::FindDistraction() const
{
	const UInventorySubsystem* Inventory = GetInventory(GetWorld());

	if (!Inventory)
	{
		return nullptr;
	}

	for (const FInventoryEntry& Entry : Inventory->GetInventory())
	{
		UItemDefinition* Item = Entry.Definition.LoadSynchronous();

		if (!Item || !Item->ItemTags.HasTag(TAG_Item_Distraction))
		{
			continue;
		}

		if (Item->WorldActorClass.IsNull())
		{
			UE_LOG(LogNevergone, Warning,
				TEXT("[ExcursionThrowComponent] %s is tagged Item.Distraction but has no WorldActorClass to fly and land as; skipped"),
				*GetNameSafe(Item));

			continue;
		}

		return Item;
	}

	return nullptr;
}

void UExcursionThrowComponent::SetAimRotation(bool bAim)
{
	ACharacter* Character = GetCharacter();

	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;

	if (!Movement)
	{
		return;
	}

	if (bAim)
	{
		bSavedOrientRotationToMovement = Movement->bOrientRotationToMovement;
		bSavedUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;

		Movement->bOrientRotationToMovement = false;
		Movement->bUseControllerDesiredRotation = true;
	}
	else
	{
		Movement->bOrientRotationToMovement = bSavedOrientRotationToMovement;
		Movement->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
	}
}

void UExcursionThrowComponent::ShowPrompt(const FText& Message) const
{
	const ACharacter* Character = GetCharacter();

	if (AExplorationPlayerController* PlayerController =
		Character ? Cast<AExplorationPlayerController>(Character->GetController()) : nullptr)
	{
		PlayerController->ShowExplorationPrompt(Message);
	}
}

ACharacter* UExcursionThrowComponent::GetCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

#undef LOCTEXT_NAMESPACE
