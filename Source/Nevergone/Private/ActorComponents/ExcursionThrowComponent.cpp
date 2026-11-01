// Copyright Xyzto Works

#include "ActorComponents/ExcursionThrowComponent.h"

#include "Nevergone.h"
#include "Actors/ExcursionThrowable.h"
#include "Characters/PlayerControllers/ExplorationPlayerController.h"
#include "Data/ItemDefinition.h"
#include "Types/ToolRingRules.h"
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
	SetTrackInventory(false);

	StopAiming();

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Aim
// ---------------------------------------------------------------------------

void UExcursionThrowComponent::BeginAim()
{
	// Nothing is aimed with the ring open.
	if (bAiming || bRingOpen)
	{
		return;
	}

	RefreshSelection();

	if (!GetSelectedThrowable())
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
	if (bRingOpen)
	{
		bRingOpen = false;

		RingOptions.Reset();
		RingItemRefs.Reset();

		OnRingClosed.Broadcast();

		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExcursionThrowComponent] %s closed the ring without choosing"),
			*GetNameSafe(GetOwner()));
	}

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

	RefreshSelection();

	UItemDefinition* Item = GetSelectedThrowable();

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

void UExcursionThrowComponent::CollectThrowables(TArray<FThrowableOption>& OutOptions) const
{
	OutOptions.Reset();

	const UInventorySubsystem* Inventory = GetInventory(GetWorld());

	if (!Inventory)
	{
		return;
	}

	for (const FInventoryEntry& Entry : Inventory->GetInventory())
	{
		UItemDefinition* Item = Entry.Definition.LoadSynchronous();

		if (!Item || (!Item->ItemTags.HasTag(TAG_Item_Distraction) && !Item->ItemTags.HasTag(TAG_Item_ScentBait)))
		{
			continue;
		}

		if (Item->WorldActorClass.IsNull())
		{
			UE_LOG(LogNevergone, Warning,
				TEXT("[ExcursionThrowComponent] %s is throwable but has no WorldActorClass to fly as; skipped"),
				*GetNameSafe(Item));

			continue;
		}

		// The same item can sit in more than one stack.
		FThrowableOption* Existing = OutOptions.FindByPredicate(
			[Item](const FThrowableOption& Option) { return Option.Item == Item; });

		if (Existing)
		{
			Existing->Quantity += Entry.Quantity;
		}
		else
		{
			OutOptions.Add({ Item, Entry.Quantity });
		}
	}
}

// ---------------------------------------------------------------------------
// Tool ring
// ---------------------------------------------------------------------------

void UExcursionThrowComponent::OpenRing()
{
	if (bRingOpen)
	{
		return;
	}

	if (bAiming)
	{
		StopAiming();
	}

	RefreshSelection();

	CollectThrowables(RingOptions);

	if (RingOptions.IsEmpty())
	{
		UE_LOG(LogNevergone, Log,
			TEXT("[ExcursionThrowComponent] %s has nothing to throw; the ring stays closed"),
			*GetNameSafe(GetOwner()));

		ShowPrompt(LOCTEXT("NothingToThrow", "Nothing to throw"));

		return;
	}

	TArray<FName> Names;

	for (const FThrowableOption& Option : RingOptions)
	{
		Names.Add(Option.Item->GetFName());
		RingItemRefs.Add(Option.Item);
	}

	// The ring opens on what is selected: releasing without moving keeps it.
	RingHighlight = ToolRingRules::ResolveSelection(Names,
		SelectedItem.IsValid() ? SelectedItem->GetFName() : NAME_None);

	RingPointer = FVector2D::ZeroVector;

	bRingOpen = true;

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExcursionThrowComponent] %s opened the ring with %d options, highlighting %s"),
		*GetNameSafe(GetOwner()), RingOptions.Num(), *GetNameSafe(RingOptions[RingHighlight].Item));

	OnRingOpened.Broadcast(RingOptions, RingHighlight);
}

void UExcursionThrowComponent::CloseRing()
{
	if (!bRingOpen)
	{
		return;
	}

	bRingOpen = false;

	UItemDefinition* Chosen = RingOptions.IsValidIndex(RingHighlight) ? RingOptions[RingHighlight].Item : nullptr;

	RingOptions.Reset();
	RingItemRefs.Reset();

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExcursionThrowComponent] %s chose %s on the ring"),
		*GetNameSafe(GetOwner()), *GetNameSafe(Chosen));

	OnRingClosed.Broadcast();

	ResolveSelection(Chosen);
}

bool UExcursionThrowComponent::PointRing(const FVector2D& Look)
{
	if (!bRingOpen)
	{
		return false;
	}

	// Look Y grows downwards on screen, the pointer's Y grows up.
	RingPointer = ToolRingRules::AdvancePointer(RingPointer,
		FVector2D(Look.X, -Look.Y) * RingPointerScale, RingPointerRadius);

	const int32 Slice = ToolRingRules::PickSlice(RingPointer, RingOptions.Num(), RingDeadZone);

	if (Slice != INDEX_NONE && Slice != RingHighlight)
	{
		RingHighlight = Slice;

		OnRingHighlightChanged.Broadcast(RingHighlight);
	}

	return true;
}

UItemDefinition* UExcursionThrowComponent::GetSelectedThrowable() const
{
	return SelectedItem.Get();
}

void UExcursionThrowComponent::RefreshSelection()
{
	ResolveSelection(nullptr);
}

void UExcursionThrowComponent::ResolveSelection(UItemDefinition* Preferred)
{
	TArray<FThrowableOption> Options;

	CollectThrowables(Options);

	TArray<FName> Names;

	for (const FThrowableOption& Option : Options)
	{
		Names.Add(Option.Item->GetFName());
	}

	const FName Wanted = Preferred ? Preferred->GetFName()
		: (SelectedItem.IsValid() ? SelectedItem->GetFName() : NAME_None);

	const int32 Index = ToolRingRules::ResolveSelection(Names, Wanted);

	UItemDefinition* NewItem = Index != INDEX_NONE ? Options[Index].Item : nullptr;

	const int32 NewQuantity = Index != INDEX_NONE ? Options[Index].Quantity : 0;

	if (NewItem == SelectedItem.Get() && NewQuantity == SelectedQuantity)
	{
		return;
	}

	SelectedItem = NewItem;

	SelectedQuantity = NewQuantity;

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExcursionThrowComponent] %s selection is now %s x%d"),
		*GetNameSafe(GetOwner()), *GetNameSafe(NewItem), NewQuantity);

	OnSelectionChanged.Broadcast(NewItem, NewQuantity);
}

void UExcursionThrowComponent::SetTrackInventory(bool bTrack)
{
	if (bTrack == bTrackingInventory)
	{
		return;
	}

	UInventorySubsystem* Inventory = GetInventory(GetWorld());

	if (bTrack)
	{
		if (!Inventory)
		{
			return;
		}

		InventoryChangedHandle = Inventory->OnInventoryChanged.AddUObject(
			this, &UExcursionThrowComponent::HandleInventoryChanged);
	}
	else if (Inventory)
	{
		Inventory->OnInventoryChanged.Remove(InventoryChangedHandle);
	}

	bTrackingInventory = bTrack;

	if (bTrack)
	{
		RefreshSelection();
	}
}

void UExcursionThrowComponent::HandleInventoryChanged()
{
	RefreshSelection();
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
