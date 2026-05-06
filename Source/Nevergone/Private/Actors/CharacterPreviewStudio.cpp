// Copyright Xyzto Works

#include "Actors/CharacterPreviewStudio.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SkeletalMesh.h"

ACharacterPreviewStudio::ACharacterPreviewStudio()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
    PreviewMesh->SetupAttachment(Root);
    PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewMesh->SetSimulatePhysics(false);
    PreviewMesh->bReceivesDecals   = false;
    PreviewMesh->CastShadow        = false;
    PreviewMesh->VisibilityBasedAnimTickOption =
        EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    PreviewLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("PreviewLight"));
    PreviewLight->SetupAttachment(Root);
    // Prevents this light from leaking into the main scene — it only affects
    // primitives in the Capture's ShowOnlyActors list.
    PreviewLight->bAffectsWorld = false;

    Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
    Capture->SetupAttachment(Root);
    Capture->ProjectionType      = ECameraProjectionMode::Perspective;
    Capture->bCaptureEveryFrame  = true;
    Capture->bCaptureOnMovement  = false;
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

    Capture->ShowFlags.SetAtmosphere(false);
    Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetSkyLighting(false);
    Capture->ShowFlags.SetVolumetricFog(false);
    Capture->ShowFlags.SetDynamicShadows(false);
    Capture->ShowFlags.SetAmbientOcclusion(false);
}

void ACharacterPreviewStudio::BeginPlay()
{
    Super::BeginPlay();

    RenderTarget = NewObject<UTextureRenderTarget2D>(this);
    RenderTarget->InitCustomFormat(512, 1024, PF_B8G8R8A8, false);
    RenderTarget->ClearColor        = FLinearColor(0.f, 0.f, 0.f, 0.f);
    RenderTarget->bAutoGenerateMips = false;
    RenderTarget->TargetGamma       = 2.2f;
    RenderTarget->UpdateResourceImmediate(true);

    Capture->TextureTarget = RenderTarget;
    Capture->ShowOnlyActors.Add(this);

    // Cache the BP-authored capture location so adaptive distance can offset
    // from it without compounding across character switches.
    CaptureBaseLocation = Capture->GetRelativeLocation();
}

void ACharacterPreviewStudio::ShowCharacterClass(TSubclassOf<ACharacter> CharacterClass)
{
    if (!CharacterClass) { return; }

    const ACharacter* CDO = CharacterClass.GetDefaultObject();
    if (!CDO) { return; }

    const USkeletalMeshComponent* CDOMesh = CDO->GetMesh();
    if (!CDOMesh) { return; }

    USkeletalMesh* MeshAsset = CDOMesh->GetSkeletalMeshAsset();
    PreviewMesh->SetSkeletalMesh(MeshAsset);

    for (int32 i = 0; i < CDOMesh->GetNumMaterials(); ++i)
    {
        PreviewMesh->SetMaterial(i, CDOMesh->GetMaterial(i));
    }

    USkeletalMeshComponent* MutableMesh = const_cast<USkeletalMeshComponent*>(CDOMesh);
    if (TSubclassOf<UAnimInstance> AnimClass = MutableMesh->GetAnimClass())
    {
        PreviewMesh->SetAnimInstanceClass(AnimClass);
    }

    // PreviewMesh transform is whatever you set in the BP viewport — we do
    // NOT copy CDOMesh->GetRelativeTransform() because that overrides your
    // manual placement.

    const float Radius = MeshAsset ? MeshAsset->GetBounds().SphereRadius : 0.f;
    ApplyAdaptiveDistance(Radius);

    UE_LOG(LogTemp, Log,
        TEXT("[CharacterPreviewStudio] Showing '%s' | mesh: %s | radius: %.1f"),
        *CharacterClass->GetName(),
        *GetNameSafe(MeshAsset),
        Radius);
}

void ACharacterPreviewStudio::ApplyAdaptiveDistance(float SphereRadius)
{
    // Pull the camera further back along its local -X axis (the direction it
    // looks toward, away from the mesh) for characters larger than threshold.
    float ExtraDistance = 0.f;
    if (SphereRadius > BaseRadiusThreshold)
    {
        ExtraDistance = (SphereRadius - BaseRadiusThreshold) * AdaptiveDistanceScale;
    }

    // The capture's local forward in this rig is +X (it's pointed toward the
    // mesh from a position offset along +X), so "back" is -X in capture-local
    // space. Convert to relative space using the capture's current rotation.
    const FVector LocalBack = Capture->GetRelativeRotation().RotateVector(FVector(-1.f, 0.f, 0.f));
    const FVector NewLocation = CaptureBaseLocation + (LocalBack * ExtraDistance);

    Capture->SetRelativeLocation(NewLocation);
}

UTextureRenderTarget2D* ACharacterPreviewStudio::GetRenderTarget() const
{
    return RenderTarget;
}
