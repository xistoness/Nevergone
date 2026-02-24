#include "Public/Characters/Abilities/TargetingRules/FactionRule.h"
#include "GameFramework/Actor.h"

#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"

bool FactionRule::IsSatisfied(const FActionContext& Context) const
{
	ACharacterBase* Caster =
		Cast<ACharacterBase>(Context.SourceActor);

	ACharacterBase* TargetUnit =
		Cast<ACharacterBase>(Context.TargetActor);

	if (!Caster || !TargetUnit)
		return false;

	UUnitStatsComponent* CasterStats =
		Caster->GetUnitStats();

	UUnitStatsComponent* TargetStats =
		TargetUnit->GetUnitStats();

	if (!CasterStats || !TargetStats)
		return false;

	switch (Type)
	{
	case EFactionType::Self:
		return Caster == TargetUnit;

	case EFactionType::Ally:
		return CasterStats->GetAllyTeam() ==
			   TargetStats->GetAllyTeam();

	case EFactionType::Enemy:
		return CasterStats->GetEnemyTeam() !=
			   TargetStats->GetEnemyTeam();
	}

	return false;
}
