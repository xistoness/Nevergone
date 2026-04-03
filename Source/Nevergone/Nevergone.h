// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/** Main log category used across the project */
DECLARE_LOG_CATEGORY_EXTERN(LogNevergone, Log, All);

// ---------------------------------------------------------------------------
// Native Gameplay Tags
// They are available project-wide and guaranteed to exist
// before any CDO or static initializer calls RequestGameplayTag().
// ---------------------------------------------------------------------------

// Ability identity tags
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Attack_Melee)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Attack_Ranged)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Attack_Magical)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Heal_Melee)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Heal_Ranged)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Heal_AoE)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Move)

// Activation requirement tags
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Mode_Combat)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Turn_CanAct)

// State tags
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Stunned)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_AbilityOnCooldown)

// Status tags
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Status_Incapacitated)