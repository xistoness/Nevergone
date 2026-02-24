#pragma once

#include "CoreMinimal.h"

/**
 * Minimal static helper for debug drawing.
 */
struct DebugDrawHelper
{
	// Context-aware versions
	static void DrawLine(
		const UObject* WorldContext,
		const FVector& Start,
		const FVector& End,
		const FColor& Color = FColor::Red,
		float Duration = 1.0f,
		float Thickness = 1.5f
	);

	static void DrawSphere(
		const UObject* WorldContext,
		const FVector& Center,
		float Radius = 25.0f,
		const FColor& Color = FColor::Green,
		float Duration = 1.0f,
		int32 Segments = 12,
		float Thickness = 1.0f
	);

	// Ultra-short versions (fallback to GWorld)
	static void DrawLine(
		const FVector& Start,
		const FVector& End,
		const FColor& Color = FColor::Red,
		float Duration = 1.0f,
		float Thickness = 1.5f
	);

	static void DrawSphere(
		const FVector& Center,
		float Radius = 25.0f,
		const FColor& Color = FColor::Green,
		float Duration = 1.0f,
		int32 Segments = 12,
		float Thickness = 1.0f
	);
};