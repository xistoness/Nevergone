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
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_AoE,				"Ability.AoE")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_SelfCast,		"Ability.Self.Cast")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Move,			"Ability.Move")

UE_DEFINE_GAMEPLAY_TAG(TAG_Mode_Combat,				"Mode.Combat")
UE_DEFINE_GAMEPLAY_TAG(TAG_Turn_CanAct,				"Turn.CanAct")

//Debuffs
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Stunned,			"State.Stunned")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Silenced,			"State.Silenced")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Charmed,			"State.Charmed")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Rooted,			"State.Rooted")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Bleeding,			"State.Bleeding")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Poisoned,			"State.Poisoned")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Slowed,			"State.Slowed")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_STR_Down,			"State.STR.Down")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_CON_Down,			"State.CON.Down")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_DEX_Down,			"State.DEX.Down")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_FOC_Down,			"State.FOC.Down")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_KNW_Down,			"State.KNW.Down")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_TEC_Down,			"State.TEC.Down")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_EVS_Down,			"State.EVS.Down")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_SPD_Down,			"State.SPD.Down")

//Buffs
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Regen,				"State.Regen")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Shielded,			"State.Shielded")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Haste,				"State.Haste")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Respite,			"State.Respite")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Cure,				"State.Cure")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_STR_Up,			"State.STR.Up")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_CON_Up,			"State.CON.Up")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_DEX_Up,			"State.DEX.Up")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_FOC_Up,			"State.FOC.Up")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_KNW_Up,			"State.KNW.Up")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_TEC_Up,			"State.TEC.Up")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_EVS_Up,			"State.EVS.Up")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_SPD_Up,			"State.SPD.Up")
