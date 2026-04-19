// Copyright Xyzto Works

#include "World/NPCAffinitySubsystem.h"
#include "GameInstance/MyGameInstance.h"
#include "GameInstance/MySaveGame.h"

void UNPCAffinitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        GI->OnSaveLoaded.AddUObject(this, &UNPCAffinitySubsystem::SyncFromGameInstance);
        SyncFromGameInstance();

        UE_LOG(LogTemp, Log, TEXT("[NPCAffinitySubsystem] Initialized."));
    }
}

void UNPCAffinitySubsystem::RegisterNPCIfNew(const FGuid& NpcGuid, int32 InitialValue)
{
    if (!NpcGuid.IsValid()) { return; }

    // Only write if this GUID has no entry — preserves values from loaded saves.
    if (!AffinityMap.Contains(NpcGuid))
    {
        AffinityMap.Add(NpcGuid, InitialValue);

        UE_LOG(LogTemp, Log,
            TEXT("[NPCAffinitySubsystem] Registered new NPC %s with initial affinity %d."),
            *NpcGuid.ToString(), InitialValue);
    }
}

int32 UNPCAffinitySubsystem::GetAffinity(const FGuid& NpcGuid) const
{
    if (!NpcGuid.IsValid()) { return 0; }

    const int32* Value = AffinityMap.Find(NpcGuid);
    return Value ? *Value : 0;
}

void UNPCAffinitySubsystem::ChangeAffinity(const FGuid& NpcGuid, int32 Delta)
{
    if (!NpcGuid.IsValid()) { return; }

    int32& Current = AffinityMap.FindOrAdd(NpcGuid, 0);
    Current += Delta;

    UE_LOG(LogTemp, Log,
        TEXT("[NPCAffinitySubsystem] NPC %s affinity changed by %d -> %d"),
        *NpcGuid.ToString(), Delta, Current);
}

void UNPCAffinitySubsystem::SyncFromGameInstance()
{
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) { return; }

    if (const UMySaveGame* Save = GI->GetActiveSave())
    {
        AffinityMap = Save->NPCAffinityMap;
        UE_LOG(LogTemp, Log,
            TEXT("[NPCAffinitySubsystem] SyncFromGameInstance: loaded %d entries."),
            AffinityMap.Num());
    }
}

void UNPCAffinitySubsystem::FlushToGameInstance()
{
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) { return; }

    if (UMySaveGame* Save = GI->GetActiveSave())
    {
        Save->NPCAffinityMap = AffinityMap;
        UE_LOG(LogTemp, Log,
            TEXT("[NPCAffinitySubsystem] FlushToGameInstance: flushed %d entries."),
            AffinityMap.Num());
    }
}
