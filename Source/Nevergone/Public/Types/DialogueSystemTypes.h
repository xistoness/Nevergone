// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/UnitAttributeTypes.h"
#include "DialogueSystemTypes.generated.h"

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

/** Determines what attribute to check for a dialogue gate. */
UENUM(BlueprintType)
enum class EDialogueConditionType : uint8
{
    None            UMETA(DisplayName = "None"),
    AttributeMin    UMETA(DisplayName = "Attribute Min"),   // Requires player attribute >= MinValue
    GlobalFlag      UMETA(DisplayName = "Global Flag"),     // Requires a flag to be set
    NPCAffinityMin  UMETA(DisplayName = "NPC Affinity Min") // Requires NPC affinity >= MinValue
};

/** What happens when the player selects a choice. */
UENUM(BlueprintType)
enum class EDialogueEffectType : uint8
{
    None                UMETA(DisplayName = "None"),
    SetFlag             UMETA(DisplayName = "Set Flag"),             // Sets flag Param = true
    ClearFlag           UMETA(DisplayName = "Clear Flag"),           // Sets flag Param = false
    StartQuest          UMETA(DisplayName = "Start Quest"),          // Starts quest named Param
    CompleteQuest       UMETA(DisplayName = "Complete Quest"),       // Force-completes quest Param
    FailQuest           UMETA(DisplayName = "Fail Quest"),           // Force-fails quest Param
    AdvanceQuestStep    UMETA(DisplayName = "Advance Quest Step"),   // Param = QuestName, Value = StepIndex
    ChangeAffinity      UMETA(DisplayName = "Change Affinity"),      // Adds Value to NPC affinity
    RequestItem         UMETA(DisplayName = "Request Item"),         // Opens inventory to deliver item Param
    GiveItem            UMETA(DisplayName = "Give Item"),            // Gives item Param (future)
};

// ---------------------------------------------------------------------------
// Condition — gates whether a node or choice is available
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FDialogueCondition
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    EDialogueConditionType Type = EDialogueConditionType::None;

    // Used by AttributeMin: which player attribute to check.
    // Replaces the previous FName AttributeName — type-safe, no typos possible.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    EUnitAttribute AttributeToCheck = EUnitAttribute::Constitution;

    // Used by AttributeMin and NPCAffinityMin: minimum value required (inclusive).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 MinValue = 0;

    // Used by GlobalFlag: the flag that must be set.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FName FlagName;
};

// ---------------------------------------------------------------------------
// Effect — consequence of selecting a choice
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FDialogueEffect
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    EDialogueEffectType Type = EDialogueEffectType::None;

    // Meaning depends on Type:
    //   SetFlag / ClearFlag     → flag name
    //   StartQuest / CompleteQuest / FailQuest / AdvanceQuestStep → quest name
    //   RequestItem / GiveItem  → item ID
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FName Param;

    // Meaning depends on Type:
    //   AdvanceQuestStep  → step index to advance
    //   ChangeAffinity    → affinity delta (can be negative)
    //   GiveItem          → item quantity
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 Value = 0;
};

// ---------------------------------------------------------------------------
// Choice — one selectable response inside a dialogue node
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FDialogueChoice
{
    GENERATED_BODY()

    // Text shown to the player in the choice list.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FText Label;

    // Index of the next FDialogueNode in UDialogueData::Nodes (-1 = end dialogue).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 NextNodeIndex = INDEX_NONE;

    // If condition Type is not None and the condition is not met, this choice
    // is shown greyed out with LockedTooltip as a tooltip.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FDialogueCondition Condition;

    // Human-readable reason shown when the choice is locked (e.g. "Requires 10 Knowledge").
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FText LockedTooltip;

    // Effects applied when this choice is selected, before advancing to NextNode.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<FDialogueEffect> Effects;
};

// ---------------------------------------------------------------------------
// Node — one screen of dialogue (NPC speaks, player responds)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FDialogueNode
{
    GENERATED_BODY()

    // Who is speaking — shown in the nameplate above the dialogue box.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FText SpeakerName;

    // The spoken line.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FText Body;

    // Portrait shown behind the dialogue box (transparent BG, Harvest Moon style).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<UTexture2D> SpeakerPortrait;

    // Available responses. If empty, dialogue ends when the player confirms.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<FDialogueChoice> Choices;
};
