// Copyright Xyzto Works

#include "Widgets/Party/PartyMemberEntryWidget.h"

#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/PlayerUnitBase.h"
#include "Data/UnitDefinition.h"

void UPartyMemberEntryWidget::SetMemberData(const FPartyMemberData& InData)
{
    CharacterID = InData.CharacterID;

    FText DisplayName        = FText::FromString(TEXT("Unknown"));
    FText ClassName          = FText::FromString(TEXT("—"));
    TObjectPtr<UTexture2D> DisplayPortrait;

    if (InData.CharacterClass)
    {
        if (APlayerUnitBase* CDO = InData.CharacterClass->GetDefaultObject<APlayerUnitBase>())
        {
            if (const UUnitDefinition* Def = CDO->GetUnitStats()->GetDefinition())
            {
                DisplayName     = Def->DisplayName;
                ClassName       = Def->ClassName;
                DisplayPortrait = Def->DisplayPortrait;
            }
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("[PartyMemberEntryWidget] SetMemberData: %s (%s) Lv%d HP=%.0f/%.0f"),
        *DisplayName.ToString(), *ClassName.ToString(),
        InData.Level, InData.CurrentHP, InData.MaxHP);

    // HP values come directly from FPartyMemberData — the subsystem is the
    // single source of truth and guarantees these are never zero after AddPartyMember.
    BP_SetMemberData(DisplayName, ClassName, DisplayPortrait, InData.Level, InData.CurrentHP, InData.MaxHP);
}

void UPartyMemberEntryWidget::SetSelected(bool bInSelected)
{
    bSelected = bInSelected;
    BP_SetSelected(bInSelected);
}