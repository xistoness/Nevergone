// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/PartyData.h"
#include "PartyMemberEntryWidget.generated.h"

/**
 * UPartyMemberEntryWidget
 *
 * Represents a single character card in the Party Manager UI.
 * Displays name, class, level, and HP bar.
 *
 * C++ drives all data population and selection state.
 * Visual layout (panels, text, progress bar) is implemented in Blueprint.
 *
 * BP implementation contract:
 *  - BP_SetMemberData: populate all visible fields from the provided data.
 *  - BP_SetSelected: apply/remove the visual highlight for keyboard navigation.
 *  - BP_SetSlotLabel: show "ACTIVE" or "BENCH" badge (optional, cosmetic).
 */
UCLASS()
class NEVERGONE_API UPartyMemberEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	// Populates the card with data from a party member.
	// Resolves the UnitDefinition from CharacterClass CDO to get DisplayName and ClassName.
	void SetMemberData(const FPartyMemberData& InData);

	// Applies or removes the keyboard-navigation selection highlight.
	void SetSelected(bool bInSelected);

	// Returns the CharacterID of the member currently displayed in this card.
	FGuid GetCharacterID() const { return CharacterID; }

	bool IsSelected() const { return bSelected; }
	
	// Shows or hides the leader crown icon on this card.
	void SetLeader(bool bInIsLeader);

	bool IsLeader() const { return bIsLeader; }

protected:

	// Called after SetMemberData — populate text blocks, HP bar, etc.
	// DisplayName and ClassName are resolved from the UnitDefinition via CDO.
	UFUNCTION(BlueprintImplementableEvent, Category = "Party Manager")
	void BP_SetMemberData(
		const FText& DisplayName,
		const FText& ClassName,
		const UTexture2D* DisplayPortrait,
		int32 Level,
		float CurrentHP,
		float MaxHP
	);

	// Called by SetSelected — update border/highlight visuals.
	UFUNCTION(BlueprintImplementableEvent, Category = "Party Manager")
	void BP_SetSelected(bool bInSelected);

	// BP_SetLeader: show/hide the leader icon. Implement in WBP_PartyMemberEntry.
	UFUNCTION(BlueprintImplementableEvent, Category = "Party Manager")
	void BP_SetLeader(bool bInIsLeader);
	
private:

	FGuid CharacterID;
	bool bSelected = false;
	bool bIsLeader = false;
};