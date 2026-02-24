#include "Public/Characters/Abilities/TargetingRules/LineOfSightRule.h"
#include "Characters/CharacterBase.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"

bool LineOfSightRule::IsSatisfied(const FActionContext& Context) const
{
	ACharacterBase* Caster =
		Cast<ACharacterBase>(Context.SourceActor);

	AActor* Target =
		Context.TargetActor;

	if (!Caster || !Target)
		return false;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Caster);

	bool bHit =
		Caster->GetWorld()->LineTraceSingleByChannel(
			Hit,
			Caster->GetActorLocation(),
			Target->GetActorLocation(),
			ECC_Visibility,
			Params
		);

	if (!bHit)
		return true;

	return Hit.GetActor() == Target;
}