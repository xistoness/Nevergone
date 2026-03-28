#include "Public/Characters/Abilities/TargetingRules/LineOfSightRule.h"
#include "Characters/CharacterBase.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

bool LineOfSightRule::IsSatisfied(const FActionContext& Context) const
{
	ACharacterBase* Caster = Cast<ACharacterBase>(Context.SourceActor);
	AActor* Target = Context.TargetActor;

	if (!Caster || !Target)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LineOfSightRule), true);
	Params.AddIgnoredActor(Caster);

	const bool bHit = Caster->GetWorld()->LineTraceSingleByChannel(
		Hit,
		Caster->GetActorLocation(),
		Target->GetActorLocation(),
		ECC_Visibility,
		Params
	);

	if (!bHit)
	{
		return true;
	}

	return Hit.GetActor() == Target;
}