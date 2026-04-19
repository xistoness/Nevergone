// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/DialogueSystemTypes.h"
#include "DialogueData.generated.h"

/**
 * UDialogueData
 *
 * A complete conversation graph stored as a DataAsset.
 * Node 0 is always the entry point. Branching is driven by FDialogueChoice::NextNodeIndex.
 * Assign this asset to ADialogueNPC::DialogueAsset in the editor.
 */
UCLASS(BlueprintType)
class NEVERGONE_API UDialogueData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	// All nodes in this conversation. Node at index 0 is the entry point.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dialogue")
	TArray<FDialogueNode> Nodes;

	// Optional condition checked before starting this conversation at all.
	// If not met, the NPC interaction is silently ignored.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dialogue")
	FDialogueCondition StartCondition;
};