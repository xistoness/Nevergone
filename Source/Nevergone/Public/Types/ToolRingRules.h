// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// ToolRingRules
//
// The pure part of the tool ring (UExcursionThrowComponent), kept free of
// the world so it can be tested. The ring is read through a virtual pointer
// that the look input pushes around the centre, mouse and gamepad alike.
//
// Pointer space: X to the right, Y up. Slice 0 sits at the top and the rest
// follow clockwise.
// ---------------------------------------------------------------------------
namespace ToolRingRules
{
	// Moves the pointer by Delta, keeping it inside MaxRadius.
	NEVERGONE_API FVector2D AdvancePointer(const FVector2D& Pointer, const FVector2D& Delta, float MaxRadius);

	// The slice the pointer is over, or INDEX_NONE inside the dead zone (or
	// without slices): the highlight stays where it was.
	NEVERGONE_API int32 PickSlice(const FVector2D& Pointer, int32 SliceCount, float DeadZoneRadius);

	// Where the selection lands when the list of what can be thrown
	// changes: the same item while it is still there, otherwise the first
	// available one, otherwise nothing (INDEX_NONE). Items are compared by
	// name.
	NEVERGONE_API int32 ResolveSelection(const TArray<FName>& Available, FName Selected);
}
