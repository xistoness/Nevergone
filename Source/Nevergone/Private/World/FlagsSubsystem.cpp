// Copyright Xyzto Works

#include "World/FlagsSubsystem.h"
#include "GameInstance/MyGameInstance.h"

void UFlagsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Bind to save-loaded event so flags stay in sync across slots
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        GI->OnSaveLoaded.AddUObject(this, &UFlagsSubsystem::SyncFromGameInstance);
        SyncFromGameInstance();

        UE_LOG(LogTemp, Log, TEXT("[FlagsSubsystem] Initialized and bound to OnSaveLoaded."));
    }
}

bool UFlagsSubsystem::HasFlag(FName Flag) const
{
    const bool* Value = Flags.Find(Flag);
    return Value && *Value;
}

void UFlagsSubsystem::SetFlag(FName Flag)
{
    UE_LOG(LogTemp, Log, TEXT("[FlagsSubsystem] SetFlag: %s"), *Flag.ToString());
    Flags.Add(Flag, true);
}

void UFlagsSubsystem::ClearFlag(FName Flag)
{
    UE_LOG(LogTemp, Log, TEXT("[FlagsSubsystem] ClearFlag: %s"), *Flag.ToString());
    Flags.Add(Flag, false);
}

void UFlagsSubsystem::SyncFromGameInstance()
{
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) { return; }

    // Mirror the authoritative flag map from GameInstance into our cache
    Flags.Empty();
    for (FName FlagName : TSet<FName>())
    {
        // Iterate all known flags via MyGameInstance public getter
    }

    // MyGameInstance exposes GetGlobalFlag per-key but not the full map.
    // We read the full map directly from the active save instead.
    if (const UMySaveGame* Save = GI->GetActiveSave())
    {
        Flags = Save->GlobalFlags;
        UE_LOG(LogTemp, Log, TEXT("[FlagsSubsystem] SyncFromGameInstance: loaded %d flags."), Flags.Num());
    }
}

void UFlagsSubsystem::FlushToGameInstance()
{
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) { return; }

    // Clear first so removed flags don't persist in the GI map
    GI->GlobalFlags.Empty();

    for (const TPair<FName, bool>& Pair : Flags)
    {
        GI->SetGlobalFlag(Pair.Key, Pair.Value);
    }

    UE_LOG(LogTemp, Log, TEXT("[FlagsSubsystem] FlushToGameInstance: flushed %d flags."), Flags.Num());
}

void UFlagsSubsystem::Reset()
{
    Flags.Empty();
    FlushToGameInstance();

    UE_LOG(LogTemp, Log, TEXT("[FlagsSubsystem] Reset: all flags cleared."));
}
