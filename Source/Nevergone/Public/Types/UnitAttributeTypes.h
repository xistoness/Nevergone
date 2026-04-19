// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UnitAttributeTypes.generated.h"

// ---------------------------------------------------------------------------
// EUnitAttribute
//
// Canonical enum identifying one of the 8 base unit attributes.
// Defined here (separate from UnitStatsComponent.h) so dialogue condition
// types can reference it without pulling in the full stats component header,
// which would create a circular dependency.
//
// GetAttributeValue(const FUnitAttributes&, EUnitAttribute) is defined inline
// in UnitStatsComponent.h, immediately after FUnitAttributes, because the
// function body requires the struct to be fully defined.
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EUnitAttribute : uint8
{
    Constitution    UMETA(DisplayName = "Constitution"),
    Strength        UMETA(DisplayName = "Strength"),
    Dexterity       UMETA(DisplayName = "Dexterity"),
    Knowledge       UMETA(DisplayName = "Knowledge"),
    Focus           UMETA(DisplayName = "Focus"),
    Technique       UMETA(DisplayName = "Technique"),
    Evasiveness     UMETA(DisplayName = "Evasiveness"),
    Speed           UMETA(DisplayName = "Speed"),
};
