// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UnitStatsComponent.h"
#include "ActorComponents/CharacterModeComponent.h"
#include "Interfaces/BattleInputReceiver.h"
#include "Types/BattleTypes.h"
#include "Types/CharacterTypes.h"
#include "BattleModeComponent.generated.h"

class UAbilityData;
class UAbilityPreviewRenderer;
class UActionResolver;
class UBattleGameplayAbility;
class UBattleMovementAbility;
class UGameplayAbility;
class ACharacterBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActionUseStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActionUseFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionPointsDepleted, ACharacterBase*, Character);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class NEVERGONE_API UBattleModeComponent : public UCharacterModeComponent, public IBattleInputReceiver
{
	GENERATED_BODY()

public:
	UBattleModeComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;
	UUnitStatsComponent* GetUnitStats() const;

	void HandleUnitDeath(ACharacterBase* OwnerCharacter);
	virtual void EnterMode() override;
	virtual void ExitMode() override;

	virtual void Input_Confirm() override;
	virtual void Input_Cancel() override;
	virtual void Input_Hover(const FVector& WorldLocation) override;
	virtual void Input_SelectNextAction() override;
	virtual void Input_SelectPreviousAction() override;

	void HandleAbilitySelected();

	bool PreviewCurrentAbilityExecution(FActionResult& ExecutionResult);
	void RenderVisualPreview();
	void PreviewCurrentAbility(FActionResult& PreviewResult);

	void ExecuteCurrentAction(const FActionContext& Context, const FActionResult& Result);

	void HandleActionStarted();
	void HandleActionFinished();

	void UpdateOccupancy(const FIntPoint& NewGridCoord) const;
	void ConsumeActionPoints(int32 ExplicitCost) const;

	const FActionContext& GetCurrentContext() const { return CurrentContext; }

	TSubclassOf<UBattleMovementAbility> GetEquippedMovementAbilityClass() const;
	const TArray<FUnitAbilityEntry>& GetGrantedBattleAbilities() const { return GrantedBattleAbilities; }

	bool SelectAbilityByIndex(int32 AbilityIndex);
	bool SelectAbilityByActionId(FName ActionId);
	bool SelectDefaultAbility();

	FOnActionUseStarted OnActionUseStarted;
	FOnActionUseFinished OnActionUseFinished;
	FOnActionPointsDepleted OnActionPointsDepleted;

protected:
	bool HasLineOfSight() const;

	void InitializeAbilitiesFromDefinition();
	void EnsureAbilityGranted(TSubclassOf<UGameplayAbility> AbilityClass);

	void SetSelectedAbilityFromEntry(const FUnitAbilityEntry& Entry);

protected:
	UPROPERTY()
	FActionContext CurrentContext;

	UPROPERTY()
	UAbilityPreviewRenderer* CurrentPreview = nullptr;

	UPROPERTY()
	UActionResolver* ActionResolver = nullptr;

	UPROPERTY()
	TArray<FUnitAbilityEntry> GrantedBattleAbilities;

	UPROPERTY()
	int32 SelectedAbilityIndex = INDEX_NONE;
};