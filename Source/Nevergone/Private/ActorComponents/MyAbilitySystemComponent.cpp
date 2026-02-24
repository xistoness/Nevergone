// Copyright Xyzto Works


#include "ActorComponents/MyAbilitySystemComponent.h"

UMyAbilitySystemComponent::UMyAbilitySystemComponent()
{
}

void UMyAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UMyAbilitySystemComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
