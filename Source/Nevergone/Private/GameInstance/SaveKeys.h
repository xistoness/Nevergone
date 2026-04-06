#pragma once

#include "CoreMinimal.h"

namespace SaveKeys
{
	// Actor-level data (transform, controller, camera, etc)
	static const FName Actor(TEXT("Actor"));

	// Unit stats, level, HP, attributes
	static const FName UnitStats(TEXT("UnitStats"));
	
	// EncounterVolume
	static const FName EncounterVolume(TEXT("EncounterVolume"));

	// Mid-combat session state (unit HP/AP/status effects/cooldowns)
	static const FName CombatSession(TEXT("CombatSession"));

	// Test / debug actors
	static const FName TestInteract(TEXT("TestInteract"));
}