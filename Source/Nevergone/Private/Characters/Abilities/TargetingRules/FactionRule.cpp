#include "Characters/Abilities/TargetingRules/FactionRule.h"

#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"

bool FactionRule::IsSatisfied(const FActionContext& Context) const
{
	ACharacterBase* Caster = Cast<ACharacterBase>(Context.SourceActor);
	ACharacterBase* TargetUnit = Cast<ACharacterBase>(Context.TargetActor);

	if (!Caster || !TargetUnit)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactionRule]: Invalid Caster or TargetUnit"));
		return false;
	}

	UUnitStatsComponent* CasterStats = Caster->GetUnitStats();
	UUnitStatsComponent* TargetStats = TargetUnit->GetUnitStats();

	if (!CasterStats || !TargetStats)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactionRule]: No CasterStats or TargetStats found"));
		return false;
	}

	switch (Type)
	{
	case EFactionType::Self:
		return Caster == TargetUnit;

	case EFactionType::Ally:
		return CasterStats->GetAllyTeam() == TargetStats->GetAllyTeam();

	case EFactionType::Enemy:
		return TargetStats->GetEnemyTeam() == CasterStats->GetAllyTeam();
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[FactionRule]: No valid EFactionType found"));
	return false;
}