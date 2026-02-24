// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "ActorComponents/CharacterModeComponent.h"
#include "Interfaces/BattleInputReceiver.h"
#include "Interfaces/GridAware.h"
#include "Types/BattleTypes.h"
#include "BattleModeComponent.generated.h"

class UGridManager;
class UActionResolver;
class UAbilityData;
class UBattleGameplayAbility;
class UAbilityPreviewRenderer;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class NEVERGONE_API UBattleModeComponent : public UCharacterModeComponent, public IBattleInputReceiver
{
	GENERATED_BODY()

public:	
	UBattleModeComponent();
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnActionPointsDepleted, ACharacterBase*);
	FOnActionPointsDepleted OnActionPointsDepleted;

	virtual void TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual void EnterMode() override;
	virtual void ExitMode() override;
	void HandleAbilitySelected();
	bool PreviewCurrentAbilityExecution(FActionResult& ExecutionResult);
	void RenderVisualPreview();
	void PreviewCurrentAbility(FActionResult& PreviewResult);

	virtual void Input_Confirm() override;
	virtual void Input_Cancel() override;
	virtual void Input_Hover(const FVector& WorldLocation) override;
	
	void SetDefaultMoveAction();
	void ExecuteCurrentAction(const FActionContext& Context, const FActionResult& Result);
	void UpdateOccupancy() const;
	void ConsumeActionPoints() const;

protected:
		
	bool HasLineOfSight() const;
	
	UPROPERTY()
	FActionContext CurrentContext;
	
	UPROPERTY()
	UAbilityPreviewRenderer* CurrentPreview;
	
	UPROPERTY()
	UAbilityData* DefaultMoveAbilityData;
	
	UPROPERTY(EditDefaultsOnly, Category="Battle|Default")
	TSubclassOf<UBattleGameplayAbility> DefaultMoveAbilityClass;
	
	UPROPERTY()
	TSubclassOf<UBattleGameplayAbility> SelectedAbilityClass;

	UPROPERTY()
	UActionResolver* ActionResolver;

};
