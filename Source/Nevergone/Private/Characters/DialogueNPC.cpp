// Copyright Xyzto Works

#include "Characters/DialogueNPC.h"

#include "ActorComponents/InteractableComponent.h"
#include "Data/DialogueData.h"
#include "World/DialogueSubsystem.h"
#include "ActorComponents/SaveableComponent.h"
#include "World/NPCAffinitySubsystem.h"

ADialogueNPC::ADialogueNPC()
{
    // Static NPC — no per-frame logic needed.
    PrimaryActorTick.bCanEverTick = false;
    InteractableComponent = CreateDefaultSubobject<UInteractableComponent>(TEXT("InteractableComponent"));
}

void ADialogueNPC::BeginPlay()
{
    Super::BeginPlay();

    const FGuid MyGuid = GetNpcGuid();

    UE_LOG(LogTemp, Log, TEXT("[DialogueNPC] '%s' ready. GUID: %s | InitialAffinity: %d | EntryPoints: %d"),
        *GetName(), *MyGuid.ToString(), InitialAffinity, DialogueEntryPoints.Num());

    // Register affinity only if no save data exists for this NPC yet.
    // This preserves affinity values that were modified in a previous session.
    if (UNPCAffinitySubsystem* AffinitySys = GetGameInstance()->GetSubsystem<UNPCAffinitySubsystem>())
    {
        AffinitySys->RegisterNPCIfNew(MyGuid, InitialAffinity);
    }
}

void ADialogueNPC::Interact_Implementation(AActor* Interactor)
{
    UE_LOG(LogTemp, Log, TEXT("[DialogueNPC] '%s' interact triggered by '%s'."),
        *GetName(), *GetNameSafe(Interactor));

    if (DialogueEntryPoints.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DialogueNPC] '%s' has no DialogueEntryPoints assigned."), *GetName());
        return;
    }

    UGameInstance* GI = GetGameInstance();
    if (!GI) { return; }

    UDialogueSubsystem* DialogueSys = GI->GetSubsystem<UDialogueSubsystem>();
    if (!DialogueSys) { return; }

    // Evaluate entry points in priority order — use the first one whose condition is met.
    for (int32 i = 0; i < DialogueEntryPoints.Num(); ++i)
    {
        const FDialogueEntryPoint& Entry = DialogueEntryPoints[i];

        if (!Entry.DialogueAsset)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[DialogueNPC] '%s' entry point %d has no DialogueAsset — skipping."),
                *GetName(), i);
            continue;
        }

        if (DialogueSys->EvaluateCondition(Entry.Condition))
        {
            UE_LOG(LogTemp, Log,
                TEXT("[DialogueNPC] '%s' using entry point %d (asset: '%s', startNode: %d)."),
                *GetName(), i, *GetNameSafe(Entry.DialogueAsset), Entry.StartNodeIndex);

            DialogueSys->StartDialogue(Entry.DialogueAsset, this, Entry.StartNodeIndex);
            return;
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[DialogueNPC] '%s' no entry point condition was met — dialogue not started."),
        *GetName());
}

FGuid ADialogueNPC::GetNpcGuid() const
{
    if (SaveableComponent)
    {
        return SaveableComponent->GetOrCreateGuid();
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[DialogueNPC] '%s' has no SaveableComponent — GUID will be invalid."), *GetName());
    return FGuid();
}
