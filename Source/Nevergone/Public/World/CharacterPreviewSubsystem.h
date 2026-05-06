// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Actors/CharacterPreviewStudio.h"
#include "CharacterPreviewSubsystem.generated.h"

class UTextureRenderTarget2D;
class ACharacter;

// ---------------------------------------------------------------------------
// UCharacterPreviewSubsystem
//
// Manages a single ACharacterPreviewStudio instance. The studio is spawned
// lazily on the first ShowCharacterClass() call.
//
// To use a Blueprint child of ACharacterPreviewStudio (e.g. to override
// CameraConfig / LightConfig defaults), set StudioClass on MyGameInstance
// or pass it via SetStudioClass() before the first ShowCharacterClass() call.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API UCharacterPreviewSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    virtual void Deinitialize() override;

    // Overrides which class is spawned for the studio. Must be called before
    // the first ShowCharacterClass() (i.e. before the inventory is opened).
    // If never called, defaults to ACharacterPreviewStudio.
    void SetStudioClass(TSubclassOf<ACharacterPreviewStudio> InClass);

    void ShowCharacterClass(TSubclassOf<ACharacter> CharacterClass);

    UTextureRenderTarget2D* GetPreviewTexture() const;

private:

    bool EnsureStudio();

private:

    // Defaults to ACharacterPreviewStudio. Set via SetStudioClass() to use
    // a Blueprint child with custom CameraConfig / LightConfig Class Defaults.
    TSubclassOf<ACharacterPreviewStudio> StudioClass = ACharacterPreviewStudio::StaticClass();

    UPROPERTY()
    ACharacterPreviewStudio* Studio = nullptr;
};
