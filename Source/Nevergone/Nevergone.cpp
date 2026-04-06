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

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Attack_Melee,	"Ability.Attack.Melee")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Attack_Ranged,	"Ability.Attack.Ranged")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Attack_Magical,	"Ability.Attack.Magical")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Heal_Melee,      "Ability.Heal.Melee")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Heal_Ranged,     "Ability.Heal.Ranged")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Heal_AoE,		"Ability.Heal.AoE")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Move,			"Ability.Move")

UE_DEFINE_GAMEPLAY_TAG(TAG_Mode_Combat,				"Mode.Combat")
UE_DEFINE_GAMEPLAY_TAG(TAG_Turn_CanAct,				"Turn.CanAct")

//Debuffs
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Stunned,			"State.Stunned")
UE_DEFINE_GAMEPLAY_TAG(TAG_Status_Incapacitated,	"Status.Incapacitated")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Silenced,	"State.Silenced")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Charmed,	"State.Charmed")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Rooted,	"State.Rooted")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Bleeding,	"State.Bleeding")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Poisoned,	"State.Poisoned")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Slowed,	"State.Slowed")

//Buffs
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Regen,	"State.Regen")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Shielded,	"State.Shielded")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Haste,	"State.Haste")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Respite,	"State.Respite")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Cure,	"State.Cure")