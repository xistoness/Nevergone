#include "Debug/DebugDrawHelper.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

static UWorld* ResolveWorld(const UObject* WorldContext)
{
	if (WorldContext)
	{
		return WorldContext->GetWorld();
	}

	return GWorld;
}

void DebugDrawHelper::DrawLine(
	const UObject* WorldContext,
	const FVector& Start,
	const FVector& End,
	const FColor& Color,
	float Duration,
	float Thickness
)
{
	if (UWorld* World = ResolveWorld(WorldContext))
	{
		DrawDebugLine(
			World,
			Start,
			End,
			Color,
			false,
			Duration,
			0,
			Thickness
		);
	}
}

void DebugDrawHelper::DrawSphere(
	const UObject* WorldContext,
	const FVector& Center,
	float Radius,
	const FColor& Color,
	float Duration,
	int32 Segments,
	float Thickness
)
{
	if (UWorld* World = ResolveWorld(WorldContext))
	{
		DrawDebugSphere(
			World,
			Center,
			Radius,
			Segments,
			Color,
			false,
			Duration,
			0,
			Thickness
		);
	}
}

void DebugDrawHelper::DrawLine(
	const FVector& Start,
	const FVector& End,
	const FColor& Color,
	float Duration,
	float Thickness
)
{
	DrawLine(nullptr, Start, End, Color, Duration, Thickness);
}

void DebugDrawHelper::DrawSphere(
	const FVector& Center,
	float Radius,
	const FColor& Color,
	float Duration,
	int32 Segments,
	float Thickness
)
{
	DrawSphere(nullptr, Center, Radius, Color, Duration, Segments, Thickness);
}