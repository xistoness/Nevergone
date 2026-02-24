#pragma once

#include "Interfaces/TargetRule.h"

class TileNotOccupiedRule : public ITargetRule
{
public:
	virtual bool IsSatisfied(const FActionContext& Context) const override;
};
