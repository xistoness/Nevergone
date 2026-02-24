#pragma once

#include "Interfaces/TargetRule.h"

enum class EFactionType
{
	Enemy,
	Ally,
	Self
};

class FactionRule : public ITargetRule
{
public:


	FactionRule(EFactionType InType)
		: Type(InType)
	{}

	virtual bool IsSatisfied(const FActionContext& Context) const override;

private:
	EFactionType Type;
};
