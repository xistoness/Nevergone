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

	// Objects that muffle noise for AI hearing (walls, closed doors). Created in
	// Project Settings > Collision with default response Block; pawns, character
	// meshes and loose physics props are set to Ignore. Must match the index
	// DefaultEngine.ini assigned to the channel.
	static constexpr ECollisionChannel NoiseOcclusion =
		ECollisionChannel::ECC_GameTraceChannel3;
}
