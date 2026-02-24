#pragma once

#include "Interfaces/TargetRule.h"

class TileNotBlockedRule : public ITargetRule
{
public:
	virtual bool IsSatisfied(const FActionContext& Context) const override;
};
