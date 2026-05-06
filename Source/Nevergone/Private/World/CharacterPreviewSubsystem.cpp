// Copyright Xyzto Works

#include "World/CharacterPreviewSubsystem.h"
#include "Engine/World.h"

void UCharacterPreviewSubsystem::Deinitialize()
{
    if (IsValid(Studio))
    {
        Studio->Destroy();
        Studio = nullptr;
    }
    Super::Deinitialize();
}

void UCharacterPreviewSubsystem::SetStudioClass(TSubclassOf<ACharacterPreviewStudio> InClass)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[CharacterPreviewSubsystem] SetStudioClass called with: %s (current: %s)"),
        InClass ? *InClass->GetName() : TEXT("NULL"),
        StudioClass ? *StudioClass->GetName() : TEXT("NULL"));

    if (!InClass || InClass == StudioClass) { return; }

    StudioClass = InClass;

    // If a studio was already spawned with the old class, destroy it so the
    // next ShowCharacterClass() call respawns with the new class (and its
    // Blueprint Class Default values for CameraConfig / LightConfig).
    if (IsValid(Studio))
    {
        Studio->Destroy();
        Studio = nullptr;
        UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewSubsystem] StudioClass changed — old studio destroyed."));
    }
}

void UCharacterPreviewSubsystem::ShowCharacterClass(TSubclassOf<ACharacter> CharacterClass)
{
    if (!EnsureStudio()) { return; }
    Studio->ShowCharacterClass(CharacterClass);
}

UTextureRenderTarget2D* UCharacterPreviewSubsystem::GetPreviewTexture() const
{
    return IsValid(Studio) ? Studio->GetRenderTarget() : nullptr;
}

bool UCharacterPreviewSubsystem::EnsureStudio()
{
    if (IsValid(Studio)) { return true; }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterPreviewSubsystem] EnsureStudio: no world."));
        return false;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    Studio = World->SpawnActor<ACharacterPreviewStudio>(
        StudioClass, FVector(0.f, 0.f, -50000.f), FRotator::ZeroRotator, Params);

    UE_LOG(LogTemp, Warning,
        TEXT("[CharacterPreviewSubsystem] EnsureStudio: spawned class=%s | actor=%s"),
        StudioClass ? *StudioClass->GetName() : TEXT("NULL"),
        *GetNameSafe(Studio));

    UE_LOG(LogTemp, Log, TEXT("[CharacterPreviewSubsystem] Studio spawned: %s."), *GetNameSafe(Studio));
    return IsValid(Studio);
}
