// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_Footstep.generated.h"

class UNoiseDefinition;

// ---------------------------------------------------------------------------
// UAnimNotify_Footstep
//
// Placed on the frame a foot touches the ground. Traces down from the foot
// bone to find the surface, then emits the noise through
// UExcursionNoiseSubsystem, which plays the surface's sound variant and
// reports the noise to enemy hearing.
//
// Each instance references its own definition: the notifies on the walk
// animation point at a walk footstep, the ones on the run animation at a run
// footstep. The notify never asks the character how it is moving.
//
// Enemy footsteps use the same notify with a definition whose Loudness is 0,
// so they are heard by the player and ignored by AI.
//
// In blend spaces set Notify Trigger Mode to Highest Weighted Animation,
// otherwise every blended animation fires its own footstep.
// ---------------------------------------------------------------------------
UCLASS(meta = (DisplayName = "Footstep (Nevergone)"))
class NEVERGONE_API UAnimNotify_Footstep : public UAnimNotify
{
	GENERATED_BODY()

public:

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, Category = "Footstep")
	TObjectPtr<UNoiseDefinition> Noise;

	// Bone the surface trace starts from. Falls back to the mesh origin when
	// the bone does not exist on this skeleton.
	UPROPERTY(EditAnywhere, Category = "Footstep")
	FName FootBone = TEXT("foot_l");

	// How far below the bone the trace looks for ground, in centimetres.
	UPROPERTY(EditAnywhere, Category = "Footstep", meta = (ClampMin = "1.0"))
	float TraceDepth = 60.0f;
};
