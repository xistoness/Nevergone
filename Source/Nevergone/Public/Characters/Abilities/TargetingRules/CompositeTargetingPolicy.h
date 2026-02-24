#pragma once

#include "Interfaces/TargetRule.h"
#include "Templates/UniquePtr.h"
#include "Containers/Array.h"

class CompositeTargetingPolicy
{
public:
	void AddRule(TUniquePtr<ITargetRule> Rule)
	{
		Rules.Add(MoveTemp(Rule));
	}

	bool IsValid(const FActionContext& Context) const
	{
		for (const TUniquePtr<ITargetRule>& Rule : Rules)
		{
			if (!Rule->IsSatisfied(Context))
				return false;
		}

		return true;
	}

private:
	TArray<TUniquePtr<ITargetRule>> Rules;
};
