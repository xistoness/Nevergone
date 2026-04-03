// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PlaySFX.generated.h"

class USoundBase;
class USoundAttenuation;

/**
 * Animation notify that plays a SFX through UAudioSubsystem.
 *
 * Drop this onto any animation timeline at the exact frame where the sound
 * should fire (e.g., frame of sword impact, footstep contact, ability cast).
 *
 * Follows the same pattern as AnimNotify_DoAttackTrace in the project:
 *   - Lightweight notify (not a NotifyState — no duration)
 *   - Routes through UAudioSubsystem, never calling UGameplayStatics directly
 *   - Works for both character SFX and ability SFX
 *
 * Usage:
 *   1. Open an Animation Sequence or Montage in the editor
 *   2. Add notify track if needed
 *   3. Right-click on the track at the desired frame → Add Notify → AnimNotify_PlaySFX
 *   4. In the notify's Details panel, assign the Sound asset
 *   5. Optionally enable bAttachToOwner to follow the mesh
 */
UCLASS(meta = (DisplayName = "Play SFX (Nevergone)"))
class NEVERGONE_API UAnimNotify_PlaySFX : public UAnimNotify
{
	GENERATED_BODY()

public:

	// -----------------------------------------------------------------------
	// UAnimNotify interface
	// -----------------------------------------------------------------------

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase*       Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual FString GetNotifyName_Implementation() const override;

	// -----------------------------------------------------------------------
	// Properties — configurable per-instance in the Animation editor
	// -----------------------------------------------------------------------

	/** The sound asset to play when this notify fires */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	TObjectPtr<USoundBase> Sound;

	/**
	 * If true, the sound follows the mesh component (good for looping or
	 * sounds that should track the character's position).
	 * If false, the sound is spawned at the mesh's world location (one-shot impacts).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	bool bAttachToOwner = false;

	/**
	 * Volume multiplier applied on top of the Sound asset's base volume.
	 * Use this to tune individual animation events without modifying the asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VolumeMultiplier = 1.0f;
};