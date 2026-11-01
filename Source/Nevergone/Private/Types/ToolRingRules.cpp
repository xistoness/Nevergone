// Copyright Xyzto Works

#include "Types/ToolRingRules.h"

namespace ToolRingRules
{
	FVector2D AdvancePointer(const FVector2D& Pointer, const FVector2D& Delta, float MaxRadius)
	{
		FVector2D Result = Pointer + Delta;

		const float Length = Result.Size();

		if (MaxRadius >= 0.0f && Length > MaxRadius)
		{
			Result *= MaxRadius / Length;
		}

		return Result;
	}

	int32 PickSlice(const FVector2D& Pointer, int32 SliceCount, float DeadZoneRadius)
	{
		if (SliceCount <= 0 || Pointer.Size() < DeadZoneRadius || Pointer.IsNearlyZero())
		{
			return INDEX_NONE;
		}

		// Clockwise angle from straight up, in 0..2PI.
		float Angle = FMath::Atan2(Pointer.X, Pointer.Y);

		if (Angle < 0.0f)
		{
			Angle += UE_TWO_PI;
		}

		const float SliceAngle = UE_TWO_PI / SliceCount;

		// Slice 0 is centred on the top, so its range straddles zero.
		return FMath::FloorToInt((Angle + SliceAngle * 0.5f) / SliceAngle) % SliceCount;
	}

	int32 ResolveSelection(const TArray<FName>& Available, FName Selected)
	{
		if (Available.IsEmpty())
		{
			return INDEX_NONE;
		}

		const int32 Found = Available.IndexOfByKey(Selected);

		return Found != INDEX_NONE ? Found : 0;
	}
}
