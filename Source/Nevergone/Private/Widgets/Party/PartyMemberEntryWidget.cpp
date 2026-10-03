// Copyright Xyzto Works

#include "Widgets/Party/PartyMemberEntryWidget.h"

#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/PlayerUnitBase.h"
#include "Data/UnitDefinition.h"

// Localization namespace for all gathered strings in this translation unit.
#define LOCTEXT_NAMESPACE "PartyMemberEntry"

void UPartyMemberEntryWidget::SetMemberData(const FPartyMemberData& InData)
{
    CharacterID = InData.CharacterID;

    // Localizable fallbacks shown when the unit class CDO is missing or has no UnitDefinition.
    // The em-dash ("—") for unset class is intentionally a separate key so a translator can
    // swap it for a culture-appropriate placeholder if needed.
    FText DisplayName = LOCTEXT("UnknownUnitFallback",  "Unknown");
    FText ClassName   = LOCTEXT("UnknownClassFallback", "—");
    TObjectPtr<UTexture2D> DisplayPortrait;

    if (InData.CharacterClass)
    {
        if (APlayerUnitBase* CDO = InData.CharacterClass->GetDefaultObject<APlayerUnitBase>())
        {
            if (const UUnitDefinition* Def = CDO->GetUnitStats()->GetDefinition())
            {
                DisplayName     = Def->DisplayName;
                ClassName       = Def->GetClassName(); // localized via UEnum metadata
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

void UPartyMemberEntryWidget::SetLeader(bool bInIsLeader)
{
    bIsLeader = bInIsLeader;
    BP_SetLeader(bInIsLeader);
}

#undef LOCTEXT_NAMESPACE
