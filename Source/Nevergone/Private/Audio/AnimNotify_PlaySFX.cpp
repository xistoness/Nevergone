// Copyright Xyzto Works

#include "Audio/AnimNotify_PlaySFX.h"

#include "Audio/AudioSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogAnimNotifySFX, Log, All);

void UAnimNotify_PlaySFX::Notify(
	USkeletalMeshComponent*          MeshComp,
	UAnimSequenceBase*                Animation,
	const FAnimNotifyEventReference&  EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!Sound)
	{
		UE_LOG(LogAnimNotifySFX, Warning,
			TEXT("[Audio] AnimNotify_PlaySFX fired on '%s' with no Sound assigned."),
			*GetNameSafe(Animation));
		return;
	}

	if (!MeshComp)
	{
		return;
	}

	UWorld* World = MeshComp->GetWorld();
	if (!World)
	{
		return;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return;
	}

	UAudioSubsystem* Audio = GI->GetSubsystem<UAudioSubsystem>();
	if (!Audio)
	{
		UE_LOG(LogAnimNotifySFX, Warning,
			TEXT("[Audio] AnimNotify_PlaySFX: UAudioSubsystem not found."));
		return;
	}

	// Route the sound through the central subsystem — never call UGameplayStatics directly
	if (bAttachToOwner)
	{
		Audio->PlaySFXAttached(Sound, MeshComp);
	}
	else
	{
		Audio->PlaySFXAtLocation(Sound, MeshComp->GetComponentLocation());
	}

	UE_LOG(LogAnimNotifySFX, Verbose,
		TEXT("[Audio] AnimNotify_PlaySFX: '%s' played on '%s' (Attached=%s)"),
		*GetNameSafe(Sound),
		*GetNameSafe(MeshComp->GetOwner()),
		bAttachToOwner ? TEXT("true") : TEXT("false"));
}

FString UAnimNotify_PlaySFX::GetNotifyName_Implementation() const
{
	// Shows the sound name inline on the animation track in the editor
	// — much easier to read than "AnimNotify_PlaySFX" repeated everywhere
	if (Sound)
	{
		return FString::Printf(TEXT("SFX: %s"), *Sound->GetName());
	}

	return TEXT("SFX: (none)");
}