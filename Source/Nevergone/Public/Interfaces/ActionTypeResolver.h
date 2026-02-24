// Copyright Xyzto Works

#pragma once
#include "Characters/Abilities/AbilityData.h"

struct FActionResult;
struct FActionContext;
class UBattlefieldQuerySubsystem;

class IActionTypeResolver
{
public:
	virtual ~IActionTypeResolver() = default;

	virtual FActionResult Resolve(
		const FActionContext& Context,
		UBattlefieldQuerySubsystem* QuerySubsystem,
		bool bIsPreview
	) = 0;
};
