#pragma once

#include "Types/BattleTypes.h"

class ITargetRule
{
public:
	virtual ~ITargetRule() = default;
	
	virtual bool IsSatisfied(const FActionContext& Context) const = 0;
	

};
