// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CharacterPreviewStudio.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class USceneCaptureComponent2D;
class UDirectionalLightComponent;
class UTextureRenderTarget2D;
class ACharacter;

// ---------------------------------------------------------------------------
// ACharacterPreviewStudio
//
// Lightweight offscreen actor that renders a character preview into a
// UTextureRenderTarget2D. All visual setup is done in the Blueprint child
// viewport — position the Capture, PreviewMesh, and PreviewLight components
// manually until the framing looks right for a medium-sized character.
//
// The only runtime adjustment is adaptive camera distance: characters whose
// mesh sphere radius exceeds BaseRadiusThreshold get pulled back further
// along the Capture's local forward (-X) axis to keep them in frame.
// ---------------------------------------------------------------------------
UCLASS(Blueprintable)
class NEVERGONE_API ACharacterPreviewStudio : public AActor
{
    GENERATED_BODY()

public:

    ACharacterPreviewStudio();

    virtual void BeginPlay() override;

    // Loads the character's skeleton, materials, and AnimBlueprint from the
    // class CDO. Does NOT touch the PreviewMesh transform — whatever you set
    // in the BP viewport stays.
    void ShowCharacterClass(TSubclassOf<ACharacter> CharacterClass);

    UTextureRenderTarget2D* GetRenderTarget() const;

protected:

    // -----------------------------------------------------------------------
    // Adaptive distance config
    //
    // The only thing C++ overrides at runtime — pulls the camera back for
    // unusually large meshes so they don't fill the frame.
    // -----------------------------------------------------------------------

    // Meshes with SphereRadius below this use the Blueprint camera position as-is.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Adaptive Distance", meta = (ClampMin = "0.0"))
    float BaseRadiusThreshold = 130.f;

    // Extra units pulled back per unit of radius above the threshold.
    // Set to 0 to disable adaptive distance entirely.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Adaptive Distance", meta = (ClampMin = "0.0"))
    float AdaptiveDistanceScale = 2.5f;

    // -----------------------------------------------------------------------
    // Components
    //
    // Position these in the BP viewport — C++ does not move them.
    // -----------------------------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
    USkeletalMeshComponent* PreviewMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
    USceneCaptureComponent2D* Capture;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
    UDirectionalLightComponent* PreviewLight;

    UPROPERTY()
    UTextureRenderTarget2D* RenderTarget;

private:

    // Capture's relative location as authored in the BP viewport.
    // Cached at BeginPlay so adaptive distance can offset from it without
    // accumulating drift across character switches.
    FVector CaptureBaseLocation = FVector::ZeroVector;

    // Applies the adaptive distance offset on top of CaptureBaseLocation.
    void ApplyAdaptiveDistance(float SphereRadius);
};
