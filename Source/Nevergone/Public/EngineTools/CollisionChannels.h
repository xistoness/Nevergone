// Copyright Xyzto Works

#pragma once

#include "Engine/EngineTypes.h"

// Project collision channels
namespace NevergoneCollision
{
	static constexpr ECollisionChannel Interactable =
		ECollisionChannel::ECC_GameTraceChannel1;
	static constexpr ECollisionChannel GridTrace =
		ECollisionChannel::ECC_GameTraceChannel2;
}
