// Copyright Xyzto Works

#include "GameMode/Combat/Resolvers/ActionResolver.h"

#include "AbilitySystemComponent.h"
#include "ActorComponents/MyAbilitySystemComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/Abilities/BattleGameplayAbility.h"

const UBattleGameplayAbility* UActionResolver::ResolveAbilityObject(
    TSubclassOf<UBattleGameplayAbility> AbilityClass,
    const FActionContext& Context
) const
{
    if (!AbilityClass) { return nullptr; }

    // Try to find the live instance on the source actor's ASC first.
    // InstancedPerActor abilities store their state (including AbilityDefinition)
    // on the instance, not the CDO — so we must use the instance for accurate previews.
    if (ACharacterBase* Source = Cast<ACharacterBase>(Context.SourceActor))
    {
        if (UAbilitySystemComponent* ASC = Source->GetAbilitySystemComponent())
        {
            if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass))
            {
                if (UBattleGameplayAbility* Instance =
                    Cast<UBattleGameplayAbility>(Spec->GetPrimaryInstance()))
                {
                    // Guard: only use the live instance if AbilityDefinition has been injected.
                    // If null, the ability was granted but InjectAbilityDefinition hasn't run yet
                    // (e.g. GAS lazy-created the instance after EnterMode). Fall through to CDO
                    // so AI units can still evaluate abilities correctly on the first turn.
                    if (Instance->AbilityDefinition)
                    {
                        return Instance;
                    }

                    UE_LOG(LogTemp, Warning,
                        TEXT("[ActionResolver] Live instance of %s on %s has null AbilityDefinition — falling back to CDO"),
                        *AbilityClass->GetName(), *GetNameSafe(Source));
                }
            }
        }
    }

    // Fallback to CDO — used by AI previews where no live instance exists for
    // the acting unit, or when the ability has not been granted yet.
    return AbilityClass->GetDefaultObject<UBattleGameplayAbility>();
}

FActionResult UActionResolver::ResolvePreview(
    TSubclassOf<UBattleGameplayAbility> AbilityClass,
    const FActionContext& Context
)
{
    const UBattleGameplayAbility* Ability = ResolveAbilityObject(AbilityClass, Context);
    return Ability ? Ability->BuildPreview(Context) : FActionResult();
}

FActionResult UActionResolver::ResolveExecution(
    TSubclassOf<UBattleGameplayAbility> AbilityClass,
    const FActionContext& Context
)
{
    const UBattleGameplayAbility* Ability = ResolveAbilityObject(AbilityClass, Context);
    return Ability ? Ability->BuildExecution(Context) : FActionResult();
}