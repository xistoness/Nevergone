#pragma once
#include "Interfaces/ActionTypeResolver.h"


class RangedActionResolver : public IActionTypeResolver
{
public:
	virtual FActionResult Resolve(
		const FActionContext& Context,
		UBattlefieldQuerySubsystem* QuerySubsystem,
		bool bIsPreview
	) override;
};