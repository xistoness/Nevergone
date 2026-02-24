#pragma once

#include "Interfaces/TargetRule.h"

class MoveRangeRule : public ITargetRule
{
public:

	virtual bool IsSatisfied(const FActionContext& Context) const override;
};
