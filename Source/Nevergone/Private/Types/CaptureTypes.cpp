// Copyright Xyzto Works

#include "Types/CaptureTypes.h"

ECaptureLineBlock CaptureRules::ResolveLineBlock(bool bClosedDoorBetween, bool bSceneryBetween)
{
	if (bClosedDoorBetween)
	{
		return ECaptureLineBlock::ClosedDoor;
	}

	return bSceneryBetween ? ECaptureLineBlock::Scenery : ECaptureLineBlock::None;
}

bool CaptureRules::CanCatch(bool bWithinCatchDistance, ECaptureLineBlock Block)
{
	return bWithinCatchDistance && Block == ECaptureLineBlock::None;
}

bool CaptureRules::IsSceneryHit(ECollisionChannel ObjectType, bool bPartOfDoor)
{
	if (bPartOfDoor)
	{
		return false;
	}

	return ObjectType == ECC_WorldStatic || ObjectType == ECC_WorldDynamic;
}
