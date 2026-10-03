// Copyright Xyzto Works


#include "Audio/AnimNotify_Footstep.h"

#include "Nevergone.h"
#include "Data/NoiseDefinition.h"
#include "World/ExcursionNoiseSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

void UAnimNotify_Footstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// The subsystem only exists in game worlds, so animation previews in the
	// editor end here silently.
	UExcursionNoiseSubsystem* NoiseSubsystem = UExcursionNoiseSubsystem::Get(MeshComp);

	if (!NoiseSubsystem)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();

	if (!Noise)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[AnimNotify_Footstep] %s fired a footstep on %s with no Noise assigned"),
			*GetNameSafe(Owner), *GetNameSafe(Animation));

		return;
	}

	const bool bHasBone = FootBone != NAME_None && MeshComp->DoesSocketExist(FootBone);

	const FVector FootLocation = bHasBone ? MeshComp->GetSocketLocation(FootBone) : MeshComp->GetComponentLocation();

	// Start slightly above the bone: at contact the bone can sit inside the
	// floor collision, and a trace starting inside a shape misses it.
	const FVector TraceStart = FootLocation + FVector(0.0f, 0.0f, 20.0f);
	const FVector TraceEnd = FootLocation - FVector(0.0f, 0.0f, TraceDepth);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FootstepSurface), false, Owner);
	Params.bReturnPhysicalMaterial = true;

	FHitResult Hit;

	const bool bHitGround = MeshComp->GetWorld()->LineTraceSingleByChannel(
		Hit, TraceStart, TraceEnd, ECC_Visibility, Params);

	const FVector NoiseLocation = bHitGround ? FVector(Hit.ImpactPoint) : FootLocation;

	const UPhysicalMaterial* Surface = bHitGround ? Hit.PhysMaterial.Get() : nullptr;

	NoiseSubsystem->EmitNoise(Noise, NoiseLocation, Owner, Surface);
}

FString UAnimNotify_Footstep::GetNotifyName_Implementation() const
{
	return Noise ? FString::Printf(TEXT("Footstep: %s"), *Noise->GetName()) : TEXT("Footstep: (none)");
}
