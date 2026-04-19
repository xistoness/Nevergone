// Copyright Xyzto Works

#include "World/DialogueSubsystem.h"

#include "Data/DialogueData.h"
#include "Types/DialogueSystemTypes.h"
#include "Types/UnitAttributeTypes.h"
#include "Characters/DialogueNPC.h"
#include "ActorComponents/ExplorationModeComponent.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "World/FlagsSubsystem.h"
#include "World/NPCAffinitySubsystem.h"
#include "World/QuestSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

bool UDialogueSubsystem::StartDialogue(UDialogueData* DialogueData, ADialogueNPC* NPC, int32 StartNodeIndex)
{
    if (bSessionActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DialogueSubsystem] StartDialogue: session already active — ignoring."));
        return false;
    }

    if (!DialogueData || DialogueData->Nodes.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DialogueSubsystem] StartDialogue: invalid or empty DialogueData."));
        return false;
    }

    if (!DialogueData->Nodes.IsValidIndex(StartNodeIndex))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[DialogueSubsystem] StartDialogue: StartNodeIndex %d is out of range (%d nodes) for '%s'."),
            StartNodeIndex, DialogueData->Nodes.Num(), *GetNameSafe(DialogueData));
        return false;
    }

    // Set context before evaluating conditions so NPCAffinityMin checks work correctly.
    ActiveDialogueData = DialogueData;
    ActiveNPC = NPC;
    bSessionActive = true;

    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
    {
        if (UExplorationModeComponent* Explore = Pawn->FindComponentByClass<UExplorationModeComponent>())
        {
            Explore->ExitMode();
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("[DialogueSubsystem] StartDialogue: starting '%s' with NPC '%s' at node %d."),
        *GetNameSafe(DialogueData), *GetNameSafe(NPC), StartNodeIndex);

    OnDialogueStarted.Broadcast();
    DisplayNode(StartNodeIndex);
    return true;
}

void UDialogueSubsystem::SelectChoice(int32 ChoiceIndex)
{
    if (!bSessionActive || !ActiveDialogueData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DialogueSubsystem] SelectChoice: no active session."));
        return;
    }

    FDialogueNode Node;
    if (!GetCurrentNode(Node))
    {
        UE_LOG(LogTemp, Error, TEXT("[DialogueSubsystem] SelectChoice: current node is null."));
        return;
    }

    if (Node.Choices.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[DialogueSubsystem] SelectChoice: no choices — ending dialogue."));
        EndDialogue();
        return;
    }

    if (!Node.Choices.IsValidIndex(ChoiceIndex))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[DialogueSubsystem] SelectChoice: index %d out of range (%d choices)."),
            ChoiceIndex, Node.Choices.Num());
        return;
    }

    const FDialogueChoice& Choice = Node.Choices[ChoiceIndex];

    if (!EvaluateCondition(Choice.Condition))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[DialogueSubsystem] SelectChoice: choice %d is locked — condition not met."),
            ChoiceIndex);
        return;
    }

    UE_LOG(LogTemp, Log,
        TEXT("[DialogueSubsystem] SelectChoice: choice %d selected, applying %d effect(s)."),
        ChoiceIndex, Choice.Effects.Num());

    ApplyEffects(Choice.Effects);

    if (Choice.NextNodeIndex == INDEX_NONE)
    {
        EndDialogue();
    }
    else
    {
        DisplayNode(Choice.NextNodeIndex);
    }
}

void UDialogueSubsystem::EndDialogue()
{
    UE_LOG(LogTemp, Log, TEXT("[DialogueSubsystem] EndDialogue."));

    bSessionActive = false;
    ActiveDialogueData = nullptr;
    ActiveNPC = nullptr;
    CurrentNodeIndex = INDEX_NONE;

    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
    {
        if (UExplorationModeComponent* Explore = Pawn->FindComponentByClass<UExplorationModeComponent>())
        {
            Explore->EnterMode();
        }
    }

    OnDialogueEnded.Broadcast();
}

bool UDialogueSubsystem::EvaluateCondition(const FDialogueCondition& Condition) const
{
    if (Condition.Type == EDialogueConditionType::None)
    {
        return true;
    }

    if (Condition.Type == EDialogueConditionType::GlobalFlag)
    {
        const UFlagsSubsystem* Flags = GetGameInstance()->GetSubsystem<UFlagsSubsystem>();
        if (!Flags) { return false; }
        return Flags->HasFlag(Condition.FlagName);
    }

    if (Condition.Type == EDialogueConditionType::AttributeMin)
    {
        const UUnitStatsComponent* Stats = GetPlayerStatsComponent();
        if (!Stats)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[DialogueSubsystem] EvaluateCondition: player has no UnitStatsComponent."));
            return false;
        }

        const int32 AttrValue = GetAttributeValue(Stats->Attributes, Condition.AttributeToCheck);
        const bool bMet = AttrValue >= Condition.MinValue;

        UE_LOG(LogTemp, Log,
            TEXT("[DialogueSubsystem] EvaluateCondition AttributeMin: attribute=%d value=%d minRequired=%d met=%s"),
            (int32)Condition.AttributeToCheck, AttrValue, Condition.MinValue,
            bMet ? TEXT("true") : TEXT("false"));

        return bMet;
    }

    if (Condition.Type == EDialogueConditionType::NPCAffinityMin)
    {
        if (!ActiveNPC)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[DialogueSubsystem] EvaluateCondition: NPCAffinityMin check with no ActiveNPC."));
            return false;
        }

        const UNPCAffinitySubsystem* Affinity = GetGameInstance()->GetSubsystem<UNPCAffinitySubsystem>();
        if (!Affinity) { return false; }

        const int32 CurrentAffinity = Affinity->GetAffinity(ActiveNPC->GetNpcGuid());
        const bool bMet = CurrentAffinity >= Condition.MinValue;

        UE_LOG(LogTemp, Log,
            TEXT("[DialogueSubsystem] EvaluateCondition NPCAffinityMin: affinity=%d minRequired=%d met=%s"),
            CurrentAffinity, Condition.MinValue, bMet ? TEXT("true") : TEXT("false"));

        return bMet;
    }

    return false;
}

bool UDialogueSubsystem::GetCurrentNode(FDialogueNode& OutNode) const
{
    if (!ActiveDialogueData || !ActiveDialogueData->Nodes.IsValidIndex(CurrentNodeIndex))
    {
        return false;
    }

    OutNode = ActiveDialogueData->Nodes[CurrentNodeIndex];
    return true;
}

void UDialogueSubsystem::ApplyEffects(const TArray<FDialogueEffect>& Effects)
{
    UGameInstance* GI = GetGameInstance();

    UFlagsSubsystem* Flags = GI->GetSubsystem<UFlagsSubsystem>();
    UNPCAffinitySubsystem* Affinity = GI->GetSubsystem<UNPCAffinitySubsystem>();
    UQuestSubsystem* Quests = GI->GetSubsystem<UQuestSubsystem>();

    for (const FDialogueEffect& Effect : Effects)
    {
        UE_LOG(LogTemp, Log, TEXT("[DialogueSubsystem] Looping through effect '%hhd'"), Effect.Type);
        switch (Effect.Type)
        {
        case EDialogueEffectType::SetFlag:
            UE_LOG(LogTemp, Log, TEXT("[DialogueSubsystem] Entering SetFlag case with Flags = %hhd"), IsValid(Flags));
            if (Flags) { Flags->SetFlag(Effect.Param); }
            break;

        case EDialogueEffectType::ClearFlag:
            if (Flags) { Flags->ClearFlag(Effect.Param); }
            break;

        case EDialogueEffectType::StartQuest:
            if (Quests) { Quests->StartQuest(Effect.Param); }
            break;

        case EDialogueEffectType::CompleteQuest:
            if (Quests) { Quests->CompleteQuest(Effect.Param); }
            break;

        case EDialogueEffectType::FailQuest:
            if (Quests) { Quests->FailQuest(Effect.Param); }
            break;

        case EDialogueEffectType::AdvanceQuestStep:
            if (Quests)
            {
                UE_LOG(LogTemp, Log,
                    TEXT("[DialogueSubsystem] AdvanceQuestStep: quest='%s' step=%d"),
                    *Effect.Param.ToString(), Effect.Value);
                Quests->AdvanceQuestStep(Effect.Param, Effect.Value);
            }
            break;

        case EDialogueEffectType::ChangeAffinity:
            if (Affinity && ActiveNPC)
            {
                Affinity->ChangeAffinity(ActiveNPC->GetNpcGuid(), Effect.Value);
            }
            break;

        case EDialogueEffectType::RequestItem:
            UE_LOG(LogTemp, Log, TEXT("[DialogueSubsystem] RequestItem: %s"), *Effect.Param.ToString());
            OnItemRequested.Broadcast(Effect.Param);
            break;

        case EDialogueEffectType::GiveItem:
            UE_LOG(LogTemp, Log,
                TEXT("[DialogueSubsystem] GiveItem: %s x%d (pending inventory system)"),
                *Effect.Param.ToString(), Effect.Value);
            break;

        default:
            break;
        }
    }
}

void UDialogueSubsystem::DisplayNode(int32 NodeIndex)
{
    if (!ActiveDialogueData->Nodes.IsValidIndex(NodeIndex))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[DialogueSubsystem] DisplayNode: index %d out of range (%d nodes)."),
            NodeIndex, ActiveDialogueData->Nodes.Num());
        EndDialogue();
        return;
    }

    CurrentNodeIndex = NodeIndex;
    const FDialogueNode& Node = ActiveDialogueData->Nodes[NodeIndex];

    UE_LOG(LogTemp, Log,
        TEXT("[DialogueSubsystem] DisplayNode %d: '%s' — %d choice(s)."),
        NodeIndex, *Node.SpeakerName.ToString(), Node.Choices.Num());

    OnDialogueNodeReady.Broadcast(Node);
}

UUnitStatsComponent* UDialogueSubsystem::GetPlayerStatsComponent() const
{
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Pawn) { return nullptr; }

    return Pawn->FindComponentByClass<UUnitStatsComponent>();
}
