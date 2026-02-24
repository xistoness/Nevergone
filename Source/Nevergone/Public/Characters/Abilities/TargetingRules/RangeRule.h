#pragma once

#include "Interfaces/TargetRule.h"

class RangeRule : public ITargetRule
{
public:
	RangeRule(int32 InMaxRange)
		: MaxRange(InMaxRange)
	{}

	virtual bool IsSatisfied(const FActionContext& Context) const override;

private:
	int32 MaxRange;
};
