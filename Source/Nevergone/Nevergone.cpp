// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nevergone.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, Nevergone, "Nevergone" );

DEFINE_LOG_CATEGORY(LogNevergone);

// ---------------------------------------------------------------------------
// Native Gameplay Tag definitions
// UE_DEFINE_GAMEPLAY_TAG registers the tag with the GameplayTagsManager
// during module startup — before any CDO is constructed — so that
// RequestGameplayTag() in constructors and static initializers is safe.
// The string here must exactly match what is used in RequestGameplayTag()
// calls throughout the project.
// ---------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Attack_Melee,  "Ability.Attack.Melee")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Attack_Ranged, "Ability.Attack.Ranged")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Heal_AoE,      "Ability.Heal.AoE")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Move,          "Ability.Move")

UE_DEFINE_GAMEPLAY_TAG(TAG_Mode_Combat,           "Mode.Combat")
UE_DEFINE_GAMEPLAY_TAG(TAG_Turn_CanAct,           "Turn.CanAct")

UE_DEFINE_GAMEPLAY_TAG(TAG_State_Stunned,         "State.Stunned")
UE_DEFINE_GAMEPLAY_TAG(TAG_Status_Incapacitated,  "Status.Incapacitated")