#pragma once

#include "Interfaces/TargetRule.h"

class LineOfSightRule : public ITargetRule
{
public:
	virtual bool IsSatisfied(const FActionContext& Context) const override;
};
