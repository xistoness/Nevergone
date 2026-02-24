#pragma once

#include "CoreMinimal.h"

namespace SaveKeys
{
	// Actor-level data (transform, controller, camera, etc)
	static const FName Actor(TEXT("Actor"));

	// Unit stats, level, HP, attributes
	static const FName UnitStats(TEXT("UnitStats"));

	// Test / debug actors
	static const FName TestInteract(TEXT("TestInteract"));
}