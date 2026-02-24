// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "ActorComponents/CharacterModeComponent.h"
#include "Interfaces/ExplorationInputReceiver.h"
#include "ExplorationModeComponent.generated.h"

class UInteractableComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class NEVERGONE_API UExplorationModeComponent : public UCharacterModeComponent, public IExplorationInputReceiver
{
	GENERATED_BODY()

public:	
	UExplorationModeComponent();
	
	virtual void TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual void EnterMode() override;
	virtual void ExitMode() override;

	virtual void Input_Interact() override;
	virtual void Input_Save() override;
	virtual void Input_Load() override;

protected:
	UPROPERTY()
	UInteractableComponent* HighlightedInteractable = nullptr;

	UPROPERTY(EditAnywhere, Category="Interaction")
	float MaxInteractionDistance = 200.0f;
	
	void UpdateInteractionTrace();
};
